#ifndef UIPROVIDER_H_
#define UIPROVIDER_H_

#include <iot_core/api/Interfaces.h>

#if __has_include("src/wifi-gateway-ui/dist/wifi-gateway-ui.generated.h")
#  include "src/wifi-gateway-ui/dist/wifi-gateway-ui.generated.h"
#else
#  include "no-ui.generated.h"
#endif

class UiProvider final : public iot_core::api::IProvider {
public:
  UiProvider() {}
  
  void setupApi(iot_core::api::IServer& server) override {
    server.on(F("/"), iot_core::api::HttpMethod::GET, [this](iot_core::api::IRequest&, iot_core::api::IResponse& response) {
      response
        .code(iot_core::api::ResponseCode::Ok)
        .contentType(iot_core::api::ContentType::TextHtml)
        .header("Content-Encoding", "gzip")
        .sendSingleBody()
      #ifdef WIFI_GATEWAY_UI_H
        .write(toolbox::strref{FPSTR(wifi_gateway_ui::HTML), wifi_gateway_ui::HTML_SIZE});
      #else
        .write(toolbox::strref{FPSTR(no_ui::HTML), no_ui::HTML_SIZE});
      #endif
    });
  }
};

#endif
