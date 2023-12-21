#pragma once
#include "app.hpp"
namespace redfish
{
void requestRoutesCertificateService(App&);
void requestRoutesHTTPSCertificate(App&);
void requestRoutesLDAPCertificate(App&);
void requestRoutesTrustStoreCertificate(App&);
} // namespace redfish
