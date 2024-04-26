...
// all the flutter channel boilerplate

void UpsignonWindows::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result)
{
        if (method_call.method_name().compare("getSystemProxyConfig") == 0)
        {
            upsignon_system_proxy::UpsignonSystemProxy::GetSystemProxyConfig(std::move(result), method_call.arguments());
        }
}
