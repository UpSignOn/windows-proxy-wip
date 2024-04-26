#include "include/upsignon_core_windows/upsignon_system_proxy.h"
#include "include/upsignon_core_windows/upsignon_flutter_helper.h"

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

	void GetContentAsBytes(HINTERNET hRequest, std::vector<BYTE>* responseData, std::string* error) {
		DWORD dwDownloadableSize = 0;
		DWORD dwTotalDownloaded = 0;
		size_t cumulativeSize = 0;

		do {
			if (!WinHttpQueryDataAvailable(hRequest, &dwDownloadableSize)) {
				*error += "Error reading content length as bytes " + std::to_string(GetLastError());
				return;
			}
			cumulativeSize += dwDownloadableSize;
			responseData->resize(cumulativeSize+1, 0);
			DWORD dwBytesRead = 0;
			if (!WinHttpReadData(hRequest, &(*responseData)[dwTotalDownloaded], dwDownloadableSize, &dwBytesRead)) {
				*error += "Error reading content as bytes " + std::to_string(GetLastError());
				return;
			}
			dwTotalDownloaded += dwBytesRead;
		} while (dwDownloadableSize > 0);
	}

	std::string GetContentAsString(HINTERNET hRequest, std::string* error) {
		DWORD dwSize = 0;
		DWORD dwDownloaded = 0;
		LPSTR pszOutBuffer;
		std::string resultContent = "";
		do
		{
			dwSize = 0;
			if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
				*error += "Error " + std::to_string(GetLastError()) + " in WinHttpQueryDataAvailable.\n";

			// Allocate space for the buffer.
			pszOutBuffer = new char[dwSize + 1];
			if (!pszOutBuffer)
			{
				*error += "Out of memory error in UpSignOnSystemProy.GetContentAsString\n";
				dwSize = 0;
			}
			else
			{
				// Read the Data.
				ZeroMemory(pszOutBuffer, dwSize + 1);

				if (!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded))
					*error += "Error " + std::to_string(GetLastError()) + " in WinHttpReadData.\n";
				else
					resultContent += pszOutBuffer;

				// Free the memory allocated to the buffer.
				delete[] pszOutBuffer;
			}

		} while (dwSize > 0);
		return resultContent;
	}

	void ApplyHeaders(const flutter::EncodableValue* inputArgs, HINTERNET hRequest) {
		if (!hRequest) return;
		const auto* arguments = std::get_if<flutter::EncodableMap>(inputArgs);
		if (arguments) {
			auto argValue = arguments->find(flutter::EncodableValue("headers"));
			if (argValue != arguments->end()) {
				auto headersRawValue = argValue->second;
				if (!headersRawValue.IsNull()) {
					const std::map<flutter::EncodableValue, flutter::EncodableValue>* headersMap = std::get_if<flutter::EncodableMap>(&headersRawValue);
					std::map<flutter::EncodableValue, flutter::EncodableValue>::const_iterator it;
					for (it = headersMap->begin(); it != headersMap->end(); it++) {
						std::string headerName = std::get<std::string>(it->first);
						std::string headerValue = std::get<std::string>(it->second);
						if (!headerValue.empty()) {
							std::string resultHeader = headerName + ": " + headerValue;
							std::wstring resultHeaderW = std::wstring(resultHeader.begin(), resultHeader.end());
							LPCWSTR pswzHeaderResult = resultHeaderW.c_str();
							WinHttpAddRequestHeaders(hRequest, pswzHeaderResult, (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);
						}
					}
				}
			}
		}
	}

	void RetrieveResponseHeaders(const flutter::EncodableValue* inputArgs, HINTERNET hRequest, flutter::EncodableMap* resultHeaders) {
		if (!hRequest) return;
		const auto* arguments = std::get_if<flutter::EncodableMap>(inputArgs);
		if (arguments) {
			auto argValue = arguments->find(flutter::EncodableValue("responseHeadersToFetch"));
			if (argValue != arguments->end()) {
				auto headersRawValue = argValue->second;
				if (!headersRawValue.IsNull()) {
					const std::vector<flutter::EncodableValue>* headersToFetch = std::get_if<std::vector<flutter::EncodableValue>>(&headersRawValue);
					for (int i = 0; i < headersToFetch->size(); i++) {
						std::string headerName = std::get<std::string>(headersToFetch->at(i));
						if (!headerName.empty()) {
							std::wstring headerNameW = std::wstring(headerName.begin(), headerName.end());
							LPCWSTR pswzHeaderName = headerNameW.c_str();
							LPWSTR pszHeaderValue = NULL;
							DWORD dwSize = 0;
							WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CUSTOM, pswzHeaderName, NULL, &dwSize, NULL);
							if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
								pszHeaderValue = new WCHAR[dwSize / sizeof(WCHAR)];
								WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CUSTOM, pswzHeaderName, pszHeaderValue, &dwSize, NULL);
								std::string responseHeaderValue = convertLPWSTRToStdString(pszHeaderValue);
								delete[] pszHeaderValue;
								resultHeaders->insert({ std::pair(flutter::EncodableValue(headerName), flutter::EncodableValue(responseHeaderValue)) });
							}
							else {
								resultHeaders->insert({ std::pair(flutter::EncodableValue(headerName), flutter::EncodableValue()) });
							}
						}
					}
				}
			}
		}
	}

	void UpsignonSystemProxy::MakeProxiedHttpRequest(std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result, const flutter::EncodableValue* arguments) {
		try {
			std::string host = GetFlutterArg<std::string>("host", arguments, "");
			std::string path = GetFlutterArg<std::string>("path", arguments, "");
			std::string scheme = GetFlutterArg<std::string>("scheme", arguments, "https");
			int port = GetFlutterArg<int>("port", arguments, scheme == "http" ? 80: 443);
			std::string method = GetFlutterArg<std::string>("method", arguments, "POST");
			std::string data = GetFlutterArg<std::string>("data", arguments, "");
			std::string acceptHeader = GetFlutterArg<std::string>("acceptHeader", arguments, "");
			int timeoutSeconds = GetFlutterArg<int>("timeoutSeconds", arguments, 10);
			
			if (host.empty()) {
				result->Success(flutter::EncodableMap{
					{flutter::EncodableValue("success"), flutter::EncodableValue(false)},
					{flutter::EncodableValue("error"), flutter::EncodableValue("EMPTY_HOST")},
				});
				return;
			}

			std::wstring hostW = std::wstring(host.begin(), host.end());
			LPCWSTR pswzServerName = hostW.c_str();

			std::wstring pathW = std::wstring(path.begin(), path.end());
			LPCWSTR pwszObjectName = pathW.c_str();

			std::wstring methodW = std::wstring(method.begin(), method.end());
			LPCWSTR pswzVerb = methodW.c_str();

			LPSTR pszData = _strdup(data.c_str());

			std::wstring acceptHeaderW = std::wstring(acceptHeader.begin(), acceptHeader.end());
			LPCWSTR ppwszAcceptType = acceptHeaderW.c_str();
			LPCWSTR ppwszAcceptTypes[2] = { ppwszAcceptType, NULL };

			INTERNET_PORT nServerPort = (INTERNET_PORT)port;

			BOOL  bResults = FALSE;
			HINTERNET hSession = NULL,
				hConnect = NULL,
				hRequest = NULL;
			DWORD dwTimeout = timeoutSeconds * 1000;
			std::string errorStr = "";
			std::string responseContent = "";
			std::vector<BYTE> responseContentAsBytes;
			DWORD dwStatusCode = 0;
			DWORD dwStatusCodeSize = sizeof(dwStatusCode);
			flutter::EncodableMap responseHeaders = flutter::EncodableMap();
			DWORD dwBytesWritten = 0;

			// Use WinHttpOpen to obtain a session handle.
			hSession = WinHttpOpen(L"UpSignOn/1.0",
				WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
				WINHTTP_NO_PROXY_NAME,
				WINHTTP_NO_PROXY_BYPASS, 0);

			// Specify an HTTP server.
			if (hSession)
				hConnect = WinHttpConnect(hSession, pswzServerName, nServerPort, 0);

			// Create an HTTP Request handle.
			if (hConnect)
				hRequest = WinHttpOpenRequest(hConnect,
					pswzVerb,
					pwszObjectName,
					NULL, // DEFAUTL HTTP/1.1 -> L"HTTP/2" ?
					WINHTTP_NO_REFERER,
					acceptHeader.empty() ? WINHTTP_DEFAULT_ACCEPT_TYPES : ppwszAcceptTypes,
					scheme != "https" ? 0 : WINHTTP_FLAG_SECURE);

			if (hRequest)
				WinHttpSetTimeouts(hRequest, dwTimeout, dwTimeout, dwTimeout, dwTimeout);
			
			if (hRequest)
				ApplyHeaders(arguments, hRequest);

			if (method != "HEAD") {
				if (hRequest)
					bResults = WinHttpSendRequest(hRequest,
						WINHTTP_NO_ADDITIONAL_HEADERS,
						0, WINHTTP_NO_REQUEST_DATA, 0,
						(DWORD)strlen(pszData), 0);

				// Write data to the server.
				if (bResults && !data.empty())
					bResults = WinHttpWriteData(hRequest, pszData,
						(DWORD)strlen(pszData),
						&dwBytesWritten);
			}
			else {
				if (hRequest)
					bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
			}

			if (bResults)
				bResults = WinHttpReceiveResponse(hRequest, NULL);

			if (bResults)
				WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &dwStatusCode, &dwStatusCodeSize, NULL);
			if (bResults)
				RetrieveResponseHeaders(arguments, hRequest, &responseHeaders);

			if (bResults && method != "HEAD")
			{
				if (acceptHeader.empty()) {
					GetContentAsBytes(hRequest, &responseContentAsBytes, &errorStr);
				}
				else {
					responseContent = GetContentAsString(hRequest, &errorStr);
				}
			}
			
			// Report any errors.
			if (!bResults)
				errorStr += "Error " + std::to_string(GetLastError()) + " has occurred.";


			// Close any open handles.
			if (hRequest) WinHttpCloseHandle(hRequest);
			if (hConnect) WinHttpCloseHandle(hConnect);
			if (hSession) WinHttpCloseHandle(hSession);
		
			result->Success(flutter::EncodableMap{
				{flutter::EncodableValue("success"), flutter::EncodableValue(true)},
				{flutter::EncodableValue("responseStatusCode"), flutter::EncodableValue((int)dwStatusCode)},
				{flutter::EncodableValue("responseContent"), flutter::EncodableValue(responseContent)},
				{flutter::EncodableValue("responseContentAsBytes"), flutter::EncodableValue(responseContentAsBytes)},
				{flutter::EncodableValue("responseHeaders"), flutter::EncodableValue(responseHeaders)},
				{flutter::EncodableValue("error"), flutter::EncodableValue(errorStr)},
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
