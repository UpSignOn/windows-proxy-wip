#ifndef UPSIGNON_FLUTTER_HELPER_H
#define UPSIGNON_FLUTTER_HELPER_H

// This must be included before many other Windows headers.
#include <windows.h>

#include <flutter/encodable_value.h>

template <typename T>
// Helper method for getting an argument from an EncodableValue.
T GetFlutterArg(const std::string arg, const flutter::EncodableValue* args, T fallback) {
	T result{ fallback };
	const auto* arguments = std::get_if<flutter::EncodableMap>(args);
	if (arguments) {
		auto result_it = arguments->find(flutter::EncodableValue(arg));
		if (result_it != arguments->end()) {
			auto val = result_it->second;
			if (!val.IsNull()) {
				result = std::get<T>(val);
			}
		}
	}
	return result;
}

#endif // UPSIGNON_FLUTTER_HELPER_H
