  Future<Map<String, dynamic>?> _sendNativeRequest({
    required String host,
    required String scheme,
    required String path,
    required String method,
    required String? data,
    required int? port,
    required String? acceptHeader,
    required int? timeoutSeconds,
    required Map<String, String?>? headers,
    required List<String>? responseHeadersToFetch,
  }) async {
    final res = await methodChannel.invokeMapMethod<String, dynamic>('makeProxiedNativeRequest', <String, dynamic>{
      'host': host,
      'scheme': scheme,
      'path': path,
      'port': port,
      'method': method,
      'data': data,
      'acceptHeader': acceptHeader,
      'timeoutSeconds': timeoutSeconds,
      'headers': headers,
      'responseHeadersToFetch': responseHeadersToFetch,
    });
    return res;
  }


  Future<ProxiedNativeRequestResponse> requestForJSON(Uri uri, String method, Map<String, dynamic>? body) async {
    if (Platform.isWindows) {
      final res = await _sendNativeRequest(
        host: uri.host,
        scheme: uri.scheme,
        path: '${uri.path}${uri.query.isNotEmpty ? '?${uri.query}' : ''}',
        method: method,
        data: body != null ? json.encode(body) : null,
        port: uri.port,
        timeoutSeconds: 10,
        acceptHeader: 'application/json',
        headers: {
          'Content-Type': 'application/json; Charset=UTF-8',
        },
        responseHeadersToFetch: [],
      );

      bool success = res!['success']! as bool;
      String error = res ['error'] as String;
      if(success) {
        int statusCode = res['responseStatusCode'] as int;
        String content = res['responseContent'] as String;
        Map<String, String?> headers = res['responseHeaders'] as Map<String, String?>;
      }
    } else {
      // not implemented
    }
  }

  Future<ProxiedNativeRequestResponse> requestForBytes(Uri uri, String method, Map<String, dynamic>? body) async {
    if (Platform.isWindows) {
      final res = await _sendNativeRequest(
        host: uri.host,
        scheme: uri.scheme,
        path: '${uri.path}${uri.query.isNotEmpty ? '?${uri.query}' : ''}',
        method: method,
        data: body != null ? json.encode(body) : null,
        port: uri.port,
        timeoutSeconds: 10,
        acceptHeader: null,
        headers: {
          'Content-Type': 'application/json; Charset=UTF-8',
        },
        responseHeadersToFetch: [],
      );

      bool success = res!['success']! as bool;
      String error = res ['error'] as String;
      if(success) {
        int statusCode = res['responseStatusCode'] as int;
        Uint8List content = res['responseContentAsBytes'] as Uint8List;
        Map<String, String?> headers = res['responseHeaders'] as Map<String, String?>;
      }
    } else {
      // not implemented
    }
  }

  Future<ProxiedNativeRequestResponse> requestForWWWAuthenticateHeader(Uri uri, String method) async {
    if (Platform.isWindows) {
      final wwwAuthHeaderName = 'WWW-Authenticate'; // case insensitive
      final res = await _sendNativeRequest(
        host: uri.host,
        scheme: uri.scheme,
        path: '${uri.path}${uri.query.isNotEmpty ? '?${uri.query}' : ''}',
        method: method,
        data: null,
        port: uri.port,
        timeoutSeconds: 10,
        acceptHeader: null,
        headers: {},
        responseHeadersToFetch: [
          wwwAuthHeaderName
        ],
      );

      bool success = res!['success']! as bool;
      String error = res ['error'] as String;
      if(success) {
        int statusCode = res['responseStatusCode'] as int;
        Uint8List content = res['responseContentAsBytes'] as Uint8List;
        Map<String, String?> headers = res['responseHeaders'] as Map<String, String?>;
        String? wwwAuthHeader = headers[wwwAuthHeaderName] as String?;
      }
    } else {
      // not implemented
    }
  }