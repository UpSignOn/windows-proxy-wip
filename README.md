**I'm trying to solve the proxy configuration issue in corporate Windows environments when writing flutter apps**
DISCLAIMER:
This is not a complete package, but only parts of native code I wrote when trying to autodetect proxy configuration on windows.

As I am not an expert C++ developer, there may be a lot of room for improvement in this code.

# IDEA 1 : using WinHttp proxy auto detection methods

This works for unauthenticated proxies.
Once you have fetched the proxy, use findProxy from dart:io (https://api.flutter.dev/flutter/dart-io/HttpClient/findProxy.html)

THIS DOES NOT WORK FOR AUTHENTICATED PROXIES SUCH AS CORPORATE ONES

# IDEA 2 : do not use dart:io, use native http requests directly

This works with authenticated corporate proxies, but the API is not consistent for all platforms.
