#ifndef S3UTIL_UPLOADER_HPP
#define S3UTIL_UPLOADER_HPP

// `aws/core/Aws.h` is not installed in the documentation environment.
// `missing-include-prefixes: ['aws/']` lets MrDocs keep parsing past
// the unresolved include and synthesize stand-ins for any names
// referenced from the forgiven prefix, so the surrounding
// declarations still produce documentation.
#include <aws/core/Aws.h>

namespace s3util {

/// Upload the contents of `path` to `bucket` on S3.
void upload(Aws::String const& bucket, char const* path);

}

#endif
