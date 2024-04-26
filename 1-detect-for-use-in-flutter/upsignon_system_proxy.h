#pragma once
#ifndef FLUTTER_PLUGIN_UPSIGNON_SYSTEM_PROXY_H_
#define FLUTTER_PLUGIN_UPSIGNON_SYSTEM_PROXY_H_


// This must be included before many other Windows headers.
#include <windows.h>

#include <flutter/method_channel.h>
#include <flutter/encodable_value.h>

namespace upsignon_system_proxy {

	class UpsignonSystemProxy {
	public:
		static void GetSystemProxyConfig(std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result, const flutter::EncodableValue* arguments);
	};

}  // namespace upsignon_system_proxy

#endif  // FLUTTER_PLUGIN_UPSIGNON_SYSTEM_PROXY_H_
