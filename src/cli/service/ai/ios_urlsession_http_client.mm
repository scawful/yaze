#include "cli/service/ai/ios_urlsession_http_client.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)

#import <Foundation/Foundation.h>

namespace yaze::cli::ios {
namespace {

NSString* ToNSString(const std::string& value) {
  if (value.empty()) {
    return @"";
  }
  return [NSString stringWithUTF8String:value.c_str()];
}

std::string ToStdString(NSString* value) {
  if (!value) {
    return {};
  }
  const char* cstr = [value UTF8String];
  return cstr ? std::string(cstr) : std::string();
}

}  // namespace

absl::StatusOr<UrlSessionHttpResponse> UrlSessionHttpRequest(
    const std::string& method, const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body, int timeout_ms) {
  if (method.empty()) {
    return absl::InvalidArgumentError("UrlSessionHttpRequest: empty method");
  }
  if (url.empty()) {
    return absl::InvalidArgumentError("UrlSessionHttpRequest: empty url");
  }

  @autoreleasepool {
    NSString* url_string = ToNSString(url);
    NSURL* ns_url = [NSURL URLWithString:url_string];
    if (!ns_url) {
      return absl::InvalidArgumentError(
          absl::StrCat("UrlSessionHttpRequest: invalid url: ", url));
    }

    NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:ns_url];
    request.HTTPMethod = ToNSString(method);
    if (timeout_ms > 0) {
      request.timeoutInterval = static_cast<NSTimeInterval>(timeout_ms) / 1000.0;
    }

    for (const auto& [key, value] : headers) {
      NSString* header_key = ToNSString(key);
      NSString* header_value = ToNSString(value);
      if (header_key.length == 0) {
        continue;
      }
      [request setValue:header_value forHTTPHeaderField:header_key];
    }

    if (!body.empty()) {
      NSData* data = [NSData dataWithBytes:body.data() length:body.size()];
      request.HTTPBody = data;
    }

    __block NSData* response_data = nil;
    __block NSURLResponse* response = nil;
    __block NSError* error = nil;

    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    NSURLSessionDataTask* task =
        [[NSURLSession sharedSession] dataTaskWithRequest:request
                                        completionHandler:^(NSData* data,
                                                            NSURLResponse* resp,
                                                            NSError* err) {
                                          response_data = data;
                                          response = resp;
                                          error = err;
                                          dispatch_semaphore_signal(sem);
                                        }];
    [task resume];

    if (timeout_ms <= 0) {
      dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    } else {
      dispatch_time_t deadline =
          dispatch_time(DISPATCH_TIME_NOW, (int64_t)timeout_ms * NSEC_PER_MSEC);
      if (dispatch_semaphore_wait(sem, deadline) != 0) {
        [task cancel];
        return absl::DeadlineExceededError("Request timed out");
      }
    }

    if (error) {
      std::string message = ToStdString(error.localizedDescription);
      if (message.empty()) {
        message = "URLSession request failed";
      }
      return absl::UnavailableError(message);
    }

    UrlSessionHttpResponse out;
    if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
      NSHTTPURLResponse* http = static_cast<NSHTTPURLResponse*>(response);
      out.status_code = static_cast<int>(http.statusCode);
      NSDictionary<NSString*, id>* all_headers = http.allHeaderFields;
      for (id key in all_headers) {
        NSString* key_str = [key isKindOfClass:[NSString class]]
                                 ? static_cast<NSString*>(key)
                                 : [key description];
        NSString* value_str =
            [[all_headers objectForKey:key] isKindOfClass:[NSString class]]
                ? static_cast<NSString*>([all_headers objectForKey:key])
                : [[all_headers objectForKey:key] description];
        out.headers[ToStdString(key_str)] = ToStdString(value_str);
      }
    }

    if (response_data) {
      out.body.assign(reinterpret_cast<const char*>(response_data.bytes),
                      static_cast<size_t>(response_data.length));
    }

    return out;
  }
}

}  // namespace yaze::cli::ios

#else

namespace yaze::cli::ios {

absl::StatusOr<UrlSessionHttpResponse> UrlSessionHttpRequest(
    const std::string&, const std::string&,
    const std::map<std::string, std::string>&, const std::string&, int) {
  return absl::UnimplementedError(
      "UrlSessionHttpRequest is only available on iOS targets");
}

}  // namespace yaze::cli::ios

#endif

