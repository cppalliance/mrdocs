#ifndef S3UTIL_UPLOADER_HPP
#define S3UTIL_UPLOADER_HPP

#include <aws/core/Aws.h>

namespace s3util {

/// Upload the contents of `path` to `bucket`.
void upload(Aws::String const& bucket, char const* path);

} // namespace s3util

#endif
