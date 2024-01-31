#include "otel.hpp"

#include "http_request.hpp"

// Otel doesn't seem to compile about sign conversion warnings, shadow, or c
// casts.  Disable the warnings for them.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wshadow"

#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wshadow"

#endif
#include <opentelemetry/sdk/trace/span_data.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "opentelemetry/semconv/http_attributes.h"
#include "opentelemetry/semconv/url_attributes.h"

#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/exporters/ostream/common_utils.h>
#include <opentelemetry/sdk/trace/exporter.h>
#include <opentelemetry/sdk/trace/processor.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/sdk/trace/tracer_context.h>
#include <opentelemetry/sdk/trace/tracer_context_factory.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/trace/provider.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/local/datagram_protocol.hpp>

#include <cstring>
#include <iostream>
#include <vector>

std::shared_ptr<opentelemetry::trace::Tracer> get_tracer(
    std::string tracer_name)
{
    auto provider = opentelemetry::trace::Provider::GetTracerProvider();
    return provider->GetTracer(tracer_name);
}

class HttpTextMapCarrier :
    public opentelemetry::context::propagation::TextMapCarrier
{
  public:
    HttpTextMapCarrier(crow::Request& reqIn) : req(reqIn) {}

    virtual std::string_view Get(std::string_view key) const noexcept override
    {
        return req.getHeaderValue(key);
    }

    virtual void Set(std::string_view key,
                     std::string_view value) noexcept override
    {
        req.addHeader(key, value);
    }

    crow::Request& req;
};

template <typename T>
concept Iterable = requires(T t) {
                       { std::begin(t) } -> std::input_or_output_iterator;
                       { std::end(t) } -> std::input_or_output_iterator;
                       // Further checks on iterators if needed, e.g.,
                       // dereferencable
                   };

void sendOtel(crow::Request& request)
{
    // start active span
    opentelemetry::trace::StartSpanOptions options;
    options.kind = opentelemetry::trace::SpanKind::kClient;

    std::string span_name = request.url().path();

    namespace SemanticConventions = opentelemetry::semconv::url;

    std::vector<
        std::pair<std::string_view, opentelemetry::common::AttributeValue>>
        attributes = {
            {SemanticConventions::kUrlFull, request.url().buffer()},
            {SemanticConventions::kUrlPath, request.url().path()},
            {SemanticConventions::kUrlFragment, request.url().fragment()},
            {SemanticConventions::kUrlScheme, request.url().scheme()}};

    HttpTextMapCarrier carrier(request);

    auto span = get_tracer("bmcweb-http-client")
                    ->StartSpan(span_name, attributes, options);
    auto scope = get_tracer("bmcweb-http-client")->WithActiveSpan(span);

    // inject current context into http header
    auto current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();
    auto prop = opentelemetry::context::propagation::GlobalTextMapPropagator::
        GetGlobalPropagator();
    prop->Inject(carrier, current_ctx);

    // TODO, get data back?
    // Does the span transfer with the request, or can we get_tracer again?
    /*
        // send http request

        if (result)
        {
            // set span attributes
            auto status_code = result.GetResponse().GetStatusCode();
            span->SetAttribute(SemanticConventions::kHttpResponseStatusCode,
                               status_code);
            result.GetResponse().ForEachHeader(
                [&span](std::string_view header_name,
                        std::string_view header_value) {
                span->SetAttribute("http.header." +
       std::string(header_name.data()), header_value); return true;
            });

            if (status_code >= 400)
            {
                span->SetStatus(StatusCode::kError);
            }
        }
        else
        {
            span->SetStatus(
                StatusCode::kError,
                "Response Status :" +
                    std::to_string(static_cast<typename std::underlying_type<
                                       http_client::SessionState>::type>(
                        result.GetSessionState())));
        }
    */
    // end span and export data
    span->End();
}

namespace bmcweb
{

class BmcwebSpanExporter final : public opentelemetry::sdk::trace::SpanExporter
{
    /*
        This class is a bmcweb specific implementation of an OpenTelemetry OTLP
       HTTP exporter [1]

        It differs from the core implementation in that:
        1. It uses boost primitives for socket communication.
        2. It removes the dependency on protocol buffers, relying only on
       nlohmann for json encoding.
        3. Rather than using HTTP for communications, it sends data to a Unix
       domain datagram socket.  It does this to guarantee that any overflows on
       high stress workloads will get dropped in the kernel.
       Each datagram packet represents one json encoded OTLP record [2]

       [1]https://opentelemetry.io/docs/specs/otel/protocol/file-exporter/
       [2]https://github.com/open-telemetry/opentelemetry-proto/blob/main/docs/specification.md#json-protobuf-encoding
    */

  private:
    static nlohmann::json::object_t encodeValueToJson(auto&& value)
    {
        nlohmann::json::object_t jsonValue;

        using BaseType = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<BaseType, std::string>)
        {
            jsonValue["stringValue"] = value;
        }
        else if constexpr (std::is_same_v<BaseType, bool>)
        {
            jsonValue["boolValue"] = value;
        }
        else if constexpr (std::is_same_v<BaseType, double>)
        {
            jsonValue["doubleValue"] = value;
        }
        else if constexpr (std::is_integral_v<BaseType>)
        {
            jsonValue["intValue"] = value;
        }
        else if constexpr (Iterable<BaseType>)
        {
            nlohmann::json::array_t array;
            for (const auto& arrayElement : value)
            {
                array.emplace_back(std::move(encodeValueToJson(arrayElement)));
            }
            jsonValue["arrayValue"] = std::move(array);
        }
        else
        {
            static_assert(false, "Unknown type");
        }
        return jsonValue;
    }

    nlohmann::json::array_t encodeAttributes(
        const std::unordered_map<
            std::string, opentelemetry::sdk::common::OwnedAttributeValue>
            attributes)
    {
        nlohmann::json::array_t attributesJson;

        for (const auto& attributeKV : attributes)
        {
            nlohmann::json::object_t attributeJson;
            attributeJson["key"] = attributeKV.first;
            attributeJson["value"] =
                std::visit([](auto&& val) { return encodeValueToJson(val); },
                           attributeKV.second);
            attributesJson.emplace_back(attributeJson);
        }
        return attributesJson;
    }

    nlohmann::json::object_t SpanToJson(
        opentelemetry::sdk::trace::SpanData& span)
    {
        nlohmann::json::object_t spanJson;

        std::array<char, 32> trace_id{};
        std::array<char, 16> span_id{};
        std::array<char, 16> parent_span_id{};

        span.GetTraceId().ToLowerBase16(trace_id);
        span.GetSpanId().ToLowerBase16(span_id);
        span.GetParentSpanId().ToLowerBase16(parent_span_id);

        spanJson["name"] = span.GetName();

        nlohmann::json::object_t context;
        context["traceId"] = std::string_view(trace_id.data(), trace_id.size());
        context["spanId"] = std::string_view(span_id.data(), span_id.size());
        spanJson["context"] = std::move(context);
        spanJson["parentId"] =
            std::string_view(parent_span_id.data(), parent_span_id.size());
        spanJson["startTimeUnixNano"] =
            span.GetStartTime().time_since_epoch().count();
        spanJson["endTimeUnixNano"] =
            (span.GetStartTime().time_since_epoch() + span.GetDuration())
                .count();
        spanJson["traceState"] =
            span.GetSpanContext().trace_state()->ToHeader();
        spanJson["kind"] = static_cast<int>(span.GetSpanKind()) + 1;

        nlohmann::json::object_t status;
        status["code"] = static_cast<int>(span.GetStatus()) + 1;
        spanJson["status"] = std::move(status);

        spanJson["attributes"] = encodeAttributes(span.GetAttributes());

        nlohmann::json::array_t events;
        for (const auto& eventKv : span.GetEvents())
        {
            nlohmann::json::object_t eventJson;
            eventJson["name"] = eventKv.GetName();
            eventJson["attributes"] = encodeAttributes(eventKv.GetAttributes());
            events.emplace_back(std::move(eventJson));
        }
        spanJson["events"] = std::move(events);

        nlohmann::json::array_t links;
        for (const auto& linkKV : span.GetLinks())
        {
            nlohmann::json::object_t linkJson;

            std::array<char, 32> link_trace_id{};

            linkKV.GetSpanContext().trace_id().ToLowerBase16(link_trace_id);
            linkJson["traceId"] =
                std::string_view(link_trace_id.data(), link_trace_id.size());

            std::array<char, 16> link_span_id{};
            linkKV.GetSpanContext().span_id().ToLowerBase16(link_span_id);
            linkJson["spanId"] =
                std::string_view(link_span_id.data(), link_span_id.size());
            linkJson["traceState"] =
                linkKV.GetSpanContext().trace_state()->ToHeader();

            linkJson["attributes"] = encodeAttributes(linkKV.GetAttributes());

            links.emplace_back(std::move(linkJson));
        }
        spanJson["links"] = std::move(links);
        return spanJson;
    }

    nlohmann::json::object_t ToJson(
        std::span<std::unique_ptr<opentelemetry::sdk::trace::Recordable>>
            records)
    {
        nlohmann::json::object_t otlp;
        nlohmann::json::array_t resourceSpans;
        nlohmann::json::object_t resourceSpan;

        // TODO Is resource expected?
        // nlohmann::json::object_t resource;
        // resource["attributes"] = encodeAttributes(????.GetAttributes());
        // resourceSpan["resource"] = std::move(resource);

        nlohmann::json::array_t scopeSpans;
        nlohmann::json::object_t scopeSpan;
        nlohmann::json::object_t scope;
        scope["name"] = "manual-test";
        scopeSpan["scope"] = std::move(scope);
        nlohmann::json::array_t spans;

        for (auto& recordable : records)
        {
            auto span = std::unique_ptr<opentelemetry::sdk::trace::SpanData>(
                static_cast<opentelemetry::sdk::trace::SpanData*>(
                    recordable.release()));
            if (span == nullptr)
            {
                continue;
            }

            spans.emplace_back(SpanToJson(*span));
        }
        scopeSpan["spans"] = std::move(spans);

        scopeSpans.emplace_back(std::move(scopeSpan));
        resourceSpan["scopeSpans"] = std::move(scopeSpans);

        resourceSpans.emplace_back(std::move(resourceSpan));
        otlp["resourceSpans"] = std::move(resourceSpans);
        return otlp;
    }

  public:
    BmcwebSpanExporter(boost::asio::io_context& io) : socket(io)
    {
        boost::asio::local::datagram_protocol::endpoint ep(
            "/tmp/bmcweb_http_client_requests.sock");
        socket.non_blocking(true);
        socket.connect(ep);
    }

    std::unique_ptr<opentelemetry::sdk::trace::Recordable>
        MakeRecordable() noexcept override
    {
        return std::unique_ptr<opentelemetry::sdk::trace::Recordable>(
            new opentelemetry::sdk::trace::SpanData());
    }

    opentelemetry::sdk::common::ExportResult Export(
        const std::span<std::unique_ptr<opentelemetry::sdk::trace::Recordable>>&
            records) noexcept override
    {
        if (isShutdown)
        {
            BMCWEB_LOG_ERROR(
                "Exporting {}  span(s) failed, exporter is shutdown",
                records.size());
            return opentelemetry::sdk::common::ExportResult::kFailure;
        }

        nlohmann::json otlp(ToJson(records));

        std::string content =
            otlp.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        boost::system::error_code ec;
        if (content.size() > 65535)
        {
            BMCWEB_LOG_WARNING("OTEL content was greater than UDP packet size");
            return opentelemetry::sdk::common::ExportResult::kSuccess;
        }
        socket.send(
            boost::asio::buffer(content),
            boost::asio::local::datagram_protocol::socket::message_flags(), ec);

        if (ec == boost::asio::error::would_block)
        {
            BMCWEB_LOG_ERROR("Logging daemon blocking");
        }
        else
        {
            if (ec)
            {
                isShutdown = true;
                return opentelemetry::sdk::common::ExportResult::kFailure;
            }
        }
        return opentelemetry::sdk::common::ExportResult::kSuccess;
    }

    bool Shutdown(std::chrono::microseconds /* timeout */) noexcept override
    {
        isShutdown = true;
        return true;
    }

    bool ForceFlush(std::chrono::microseconds /* timeout */) noexcept override
    {
        return true;
    }

  private:
    std::atomic<bool> isShutdown = false;
    boost::asio::local::datagram_protocol::socket socket;
};

class BmcwebSpanExporterFactory
{
  public:
    static std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> Create(
        boost::asio::io_context& io)
    {
        return std::make_unique<BmcwebSpanExporter>(io);
    }
};

} // namespace bmcweb

class OtelTracer
{
    // RAII object for managing registration and deregistration of OTEL handlers
  public:
    OtelTracer(boost::asio::io_context& io)
    {
        std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>>
            processors;

        std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>
            bmcwebExporter = bmcweb::BmcwebSpanExporterFactory::Create(io);

        std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>
            bmcwebProcessor =
                opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(
                    std::move(bmcwebExporter));

        processors.push_back(std::move(bmcwebProcessor));

        /*
        // In memory exporter
        namespace memory = opentelemetry::exporter::memory;

        std::shared_ptr<memory::InMemorySpanData> memoryData =
            std::make_shared<memory::InMemorySpanData>(1024);

        std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>
            inMemoryExporter =
                memory::InMemorySpanExporterFactory::Create(memoryData);
        std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>
            inMemoryProcessor =
                opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(
                    std::move(inMemoryExporter));
        processors.push_back(std::move(inMemoryProcessor));
        */

        /*
            // OTLP exporter
            namespace otlp = opentelemetry::exporter::otlp;
            otlp::OtlpHttpExporterOptions opts;
            opts.url = "http://localhost:4318/v1/traces";

            auto otlpExporter = otlp::OtlpHttpExporterFactory::Create(opts);
            auto otlpProcessor =
                trace::SimpleSpanProcessorFactory::Create(
                    std::move(otlpExporter));

            processors.push_back(std::move(otlpProcessor));
        */

        /*
                // trace exporter
                auto exporter = opentelemetry::exporter::trace::
                    OStreamSpanExporterFactory::Create();
                auto processor =
                    trace::SimpleSpanProcessorFactory::Create(
                        std::move(exporter));

                processors.push_back(std::move(processor));

        */

        // Default is an always-on sampler.
        std::unique_ptr<opentelemetry::sdk::trace::TracerContext> context =
            opentelemetry::sdk::trace::TracerContextFactory::Create(
                std::move(processors));
        std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
            opentelemetry::sdk::trace::TracerProviderFactory::Create(
                std::move(context));
        // Set the global trace provider
        opentelemetry::trace::Provider::SetTracerProvider(provider);

        // set global propagator
        opentelemetry::context::propagation::GlobalTextMapPropagator::
            SetGlobalPropagator(
                std::shared_ptr<
                    opentelemetry::context::propagation::TextMapPropagator>(
                    new opentelemetry::trace::propagation::HttpTraceContext()));
    }

    ~OtelTracer()
    {
        opentelemetry::trace::Provider::SetTracerProvider(nullptr);
    }
};

void initOtel(boost::asio::io_context& io)
{
    static OtelTracer otel(io);
}
