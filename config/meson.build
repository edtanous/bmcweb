# Gather the Configuration data

conf_data = configuration_data()
conf_data.set('BMCWEB_HTTP_REQ_BODY_LIMIT_MB', get_option('http-body-limit'))
if get_option('firmware-image-limit') > get_option('http-body-limit')
  error('Firmware image size limit should be lesser than http body limit.')
else
  conf_data.set('FIRMWARE_IMAGE_LIMIT_MB', get_option('firmware-image-limit'))
endif
xss_enabled = get_option('insecure-disable-xss')
conf_data.set10('BMCWEB_INSECURE_DISABLE_XSS_PREVENTION', xss_enabled.enabled())
enable_redfish_query = get_option('insecure-enable-redfish-query')
conf_data.set10('BMCWEB_INSECURE_ENABLE_QUERY_PARAMS', enable_redfish_query.enabled())
# enable_redfish_aggregation = get_option('redfish-aggregation')
# conf_data.set10('BMCWEB_ENABLE_REDFISH_AGGREGATION', enable_redfish_aggregation.enabled())
insecure_push_style_notification = get_option('insecure-push-style-notification')
conf_data.set10('BMCWEB_INSECURE_ENABLE_HTTP_PUSH_STYLE_EVENTING', insecure_push_style_notification.enabled())
conf_data.set('MESON_INSTALL_PREFIX', get_option('prefix'))
conf_data.set('HTTPS_PORT', get_option('https_port'))
conf_data.set('STALE_SENSOR_UPPER_LIMIT_IN_MILISECOND', get_option('stalesensor-upper-limit-milisecond'))
conf_data.set('PLATFORM_METRICS_SENSING_INTERVAL', get_option('platform-metrics-sensing-interval'))
conf_data.set('UPDATE_SERVICE_IMAGE_LOCATION', get_option('update-service-image-location'))
conf_data.set('UPDATE_SERVICE_STAGE_LOCATION', get_option('update-service-stage-location'))
conf_data.set('UPDATE_SERVICE_TASK_TIMEOUT', get_option('update-service-task-timeout'))
conf_data.set('MFG_TEST_TIMEOUT', get_option('manufacturing-test-timeout'))
conf_data.set_quoted('PLATFORM_SYSTEM_ID', get_option('platform-system-id'))
conf_data.set_quoted('PLATFORM_SYSTEM_DESCRIPTION', get_option('platform-system-description'))
conf_data.set_quoted('PLATFORM_DEVICE_PREFIX', get_option('platform-device-prefix'))
conf_data.set_quoted('PLATFORM_BMC_ID', get_option('platform-bmc-id'))
conf_data.set_quoted('PLATFORM_NETWORK_ADAPTER', get_option('platform-network-adapter'))
conf_data.set_quoted('PLATFORM_TOTAL_POWER_SENSOR_NAME', get_option('platform-total-power-sensor-name'))
conf_data.set_quoted('PLATFORM_POWER_CONTOL_SENSOR_NAME', get_option('platform-power-control-sensor-name'))
conf_data.set_quoted('PLATFORM_METRICS_ID', get_option('platform-metrics-id'))
conf_data.set_quoted('HOST_IFACE_CHANNEL', get_option('host-iface-channel'))
conf_data.set_quoted('PLATFORM_GPU_NAME_PREFIX', get_option('platform-gpu-name-prefix'))
conf_data.set_quoted('PLATFORM_CHASSIS_NAME', get_option('platform-chassis-name'))
conf_data.set_quoted('RFA_HMC_UPDATE_TARGET', get_option('rfa-hmc-update-target'))
conf_data.set_quoted('REDFISH_AGGREGATION_PREFIX', get_option('redfish-aggregation-prefix'))
conf_data.set_quoted('FW_MCTP_MAPPING_JSON', join_paths(package_datadir, 'fw_mctp_mapping.json'))
conf_data.set('BUILD_DESCRIPTION_FILE_PATH', get_option('build-description-file-path'))
conf_data.set('COMPUTER_SYSTEM_INVENTORY_PATH', get_option('computer-system-inventory-path'))
conf_data.set('OTP_PROVISIONING_STATUS_FILE_PATH', get_option('otp-provisioning-status-file-path'))
conf_data.set('BMCWEB_LOG_LEVEL', get_option('bmcweb-logging-log-level'))
conf_data.set('MIN_UCASE_CHRS', get_option('min-ucase-characters'))
conf_data.set('MIN_LCASE_CHRS', get_option('min-lcase-characters'))
conf_data.set('MIN_DIGITS', get_option('min-digits'))
conf_data.set('MIN_SPEC_CHRS', get_option('min-special-characters'))
conf_data.set('MIN_PASSWORD_LENGTH', get_option('min-password-length'))
conf_data.set('GPU_INDEX_START', get_option('gpu-index-start'))
enable_health_populate = get_option('redfish-health-populate')
conf_data.set10('BMCWEB_ENABLE_HEALTH_POPULATE', enable_health_populate.enabled())
enable_proc_mem_status = get_option('redfish-enable-proccessor-memory-status')
conf_data.set10('BMCWEB_ENABLE_PROC_MEM_STATUS', enable_proc_mem_status.enabled())
enable_multi_host = get_option('experimental-redfish-multi-computer-system')
conf_data.set10('BMCWEB_ENABLE_MULTI_HOST', enable_multi_host.enabled())
enable_http2 = get_option('experimental-http2')
conf_data.set10('BMCWEB_ENABLE_HTTP2', enable_http2.enabled())

# Logging level
loglvlopt = get_option('bmcweb-logging')
if get_option('buildtype').startswith('debug') and loglvlopt == 'disabled'
   # Override logging level as 'debug' if 'bmcweb-logging' is set as 'dsiabled'
   loglvlopt = 'debug'
endif
loglvlopt = loglvlopt.to_upper()
conf_data.set('BMCWEB_LOGGING_LEVEL', loglvlopt)

conf_data.set('TLS_CACERT_PATH', get_option('tls-cacert-path') )
conf_h_dep = declare_dependency(
    include_directories: include_directories('.'),
    sources: configure_file(
        input: 'bmcweb_config.h.in',
        output: 'bmcweb_config.h',
        configuration: conf_data
    )
)

# Configure and install systemd unit files

configure_file(input : 'bmcweb.socket.in',
               output : 'bmcweb.socket',
               install_dir: systemd_system_unit_dir,
               configuration: conf_data,
               install : true)

configure_file(input : 'bmcweb.service.in',
               output : 'bmcweb.service',
               install_dir: systemd_system_unit_dir,
               configuration: conf_data,
               install : true)

# Copy pam-webserver to etc/pam.d
install_data(
  'pam-webserver',
  install_dir: '/etc/pam.d/',
  rename: 'webserver',
)

