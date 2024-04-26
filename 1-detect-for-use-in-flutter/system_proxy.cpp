#include "upsignon_system_proxy.h"
#include "upsignon_flutter_helper.h"

#include <WinHttp.h>

// Inspired from https://gitlab.com/yamsergey/platform_proxy/-/blob/master/windows/platform_proxy_plugin.cpp?ref_type=heads
namespace upsignon_system_proxy {
	std::string convertLPWSTRToStdString(LPWSTR lpwszStr) {
		// Determine the length of the LPWSTR string
		int strLength = WideCharToMultiByte(CP_UTF8, 0, lpwszStr, -1, NULL, 0, NULL, NULL);

		// Allocate a buffer for the converted string
		char* pszStr = new char[strLength];

		// Convert the LPWSTR string to a UTF-8 encoded std::string
		WideCharToMultiByte(CP_UTF8, 0, lpwszStr, -1, pszStr, strLength, NULL, NULL);
		std::string str(pszStr);

		// Free the buffer
		delete[] pszStr;

		return str;
	}

	void trim(std::string& s) {
		// trim from start (in place)
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
			return !std::isspace(ch);
			}));
		// trim from end (in place)
		s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
			return !std::isspace(ch);
			}).base(), s.end());
	}

	void split(std::string& s, std::string delimiter, std::vector<std::string>* arrayRes) {
		std::string sCopy = s.substr(0, s.length());
		size_t pos = 0;
		std::string token;
		while ((pos = sCopy.find(delimiter)) != std::string::npos) {
			token = sCopy.substr(0, pos);
			sCopy.erase(0, pos + delimiter.length());
			trim(token);
			arrayRes->push_back(token);
		}
		if (sCopy.length() > 0) {
			arrayRes->push_back(sCopy);
		}
	}

	void GetProxyAutoConfig(LPCWSTR lpcwszUrl, HINTERNET hSession, WINHTTP_PROXY_INFO* proxyInfo) {
		// TRY FIRST WITH PROXY AUTO CONFIG FILE
		// READ THIS: https://learn.microsoft.com/en-us/windows/win32/winhttp/autoproxy-issues-in-winhttp
		// for performance warning and single proxy server at a time
		WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxyInfoIE = {};
		WINHTTP_AUTOPROXY_OPTIONS autoProxyOptions = {};
		autoProxyOptions.lpvReserved = NULL;
		autoProxyOptions.dwReserved = 0;
		autoProxyOptions.fAutoLogonIfChallenged = TRUE; // see performance warning
		autoProxyOptions.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
		autoProxyOptions.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;

		if (!WinHttpGetProxyForUrl(hSession, lpcwszUrl, &autoProxyOptions, proxyInfo)) {
			// FALLBACK TO IE PROXY CONFIG
			if (WinHttpGetIEProxyConfigForCurrentUser(&proxyInfoIE))
			{
				if (proxyInfoIE.lpszAutoConfigUrl != NULL)
				{
					autoProxyOptions.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;
					autoProxyOptions.lpszAutoConfigUrl = proxyInfoIE.lpszAutoConfigUrl;
					autoProxyOptions.dwAutoDetectFlags = NULL;
					WinHttpGetProxyForUrl(hSession, lpcwszUrl, &autoProxyOptions, proxyInfo);
				}
				else {
					//  see for dwAccessType doc https://learn.microsoft.com/fr-fr/windows/win32/api/winhttp/nf-winhttp-winhttpopen
					if(proxyInfoIE.lpszProxy)
						proxyInfo->dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
					proxyInfo->lpszProxy = proxyInfoIE.lpszProxy;
					proxyInfo->lpszProxyBypass = proxyInfoIE.lpszProxyBypass;
				}
			}
		}
		if (proxyInfoIE.lpszAutoConfigUrl != NULL)
			GlobalFree(proxyInfoIE.lpszAutoConfigUrl);
	}

	void UpsignonSystemProxy::GetSystemProxyConfig(std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result, const flutter::EncodableValue* arguments)
	{
		try {
			std::string requestedUrl = GetFlutterArg<std::string>("url", arguments, "");
			std::wstring requestedUrlW = std::wstring(requestedUrl.begin(), requestedUrl.end());
			LPCWSTR lpcwszUrl = requestedUrlW.c_str();

			HINTERNET hSession = NULL;
			WINHTTP_PROXY_INFO proxyInfo = {};
			std::vector<std::string> allSingleProxies{};

			// Create a WinHTTP session.
			hSession = WinHttpOpen(L"UpSignOn AutoProxy/1.0",
				WINHTTP_ACCESS_TYPE_NO_PROXY,
				WINHTTP_NO_PROXY_NAME,
				WINHTTP_NO_PROXY_BYPASS,
				0);
			if (hSession != NULL) {
				GetProxyAutoConfig(lpcwszUrl, hSession, &proxyInfo);

				if (proxyInfo.lpszProxy) {
					std::string proxyConfig = convertLPWSTRToStdString(proxyInfo.lpszProxy);
					// some configs look like srv-mtp.loc:8080;srv-tls.loc:8080
					split(proxyConfig, ";", &allSingleProxies);

					// some config look like http=127.0.0.1:8888;https=127.0.0.1:8888 (Charles proxy)
					for (int i = 0; i < allSingleProxies.size(); i++) {
						std::vector<std::string> schemedProxyPair{};
						split(allSingleProxies[i], "=", &schemedProxyPair);
						if (schemedProxyPair.size() == 2) {
							allSingleProxies[i] = schemedProxyPair[1];
						}
					}

					// TODO : the above parser can generate duplicates (see Charles example above)
				}

				WinHttpCloseHandle(hSession);

				// Free memory
				if (proxyInfo.lpszProxy != NULL)
					GlobalFree(proxyInfo.lpszProxy);

				if (proxyInfo.lpszProxyBypass != NULL)
					GlobalFree(proxyInfo.lpszProxyBypass);
			}

			// TODO we could also return proxy bypass
			std::string flutterProxyList = "";
			for (int i = 0; i < allSingleProxies.size(); i++) {
				flutterProxyList += "PROXY " + allSingleProxies[i] + "; ";
			}
			flutterProxyList += "DIRECT";
			result->Success(flutter::EncodableMap{
				{flutter::EncodableValue("success"), flutter::EncodableValue(true)},
				{flutter::EncodableValue("proxyConfig"), flutter::EncodableValue(flutterProxyList)},
				});

		}
		catch (const std::exception& e) {
			result->Success(flutter::EncodableMap{
				{flutter::EncodableValue("success"), flutter::EncodableValue(false)},
				{flutter::EncodableValue("error"), flutter::EncodableValue(e.what())},
				});
		}
		catch (const std::string& e) {
			result->Success(flutter::EncodableMap{
				{flutter::EncodableValue("success"), flutter::EncodableValue(false)},
				{flutter::EncodableValue("error"), flutter::EncodableValue(e)},
				});
		}
		catch (const char* e) {
			result->Success(flutter::EncodableMap{
				{flutter::EncodableValue("success"), flutter::EncodableValue(false)},
				{flutter::EncodableValue("error"), flutter::EncodableValue(e)},
				});
		}
		catch (...) {
			result->Success(flutter::EncodableMap{
				{flutter::EncodableValue("success"), flutter::EncodableValue(false)},
				{flutter::EncodableValue("error"), flutter::EncodableValue("unknown_error")},
				});
		}
	}
}  // namespace upsignon_system_proxy
