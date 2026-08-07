# Refactor MultipartUploadStrategy

## [x] Update header (includes/Response/MultipartUploadStrategy.hpp)
- Changed `MultipartParsingContext` to hold `const std::string& body` instead of `const ClientRequest& request`.
- Removed the now-unused `#include "../Request/ClientRequest.hpp"`, added `<map>`.
- Updated all helper signatures to operate on `const std::string& body` / `const std::map<...>& headers`.

## [x] Update implementation (src/Response/MultipartUploadStrategy.cpp)
- `handleMultipartUpload` now uses the passed `body` and `headers` (removed the undefined `request`).
- Replaced `request.getBody()` -> `body` and `request.getHeaders()` -> `headers` in all functions.
- `validateRequest`, `isMultipartUpload`, `extractBoundary` now take the headers map.
- `parseMultipart`, `parseNextPart`, `readPartHeaders`, `extractPartBody`, `findHeaderBlockEnd`, `isClosingDelimiter` now operate on the body string.
- Preserved the parsing algorithm and C++98 compatibility.

## [x] Update caller (src/Response/POST.cpp)
- `POST::execute()` unchanged (already used new signature).
- `POST::handleMultipartRequest` (unused leftover) updated to the new signature to keep the code compiling.

## [x] Build
- `make` succeeds cleanly with `-Wall -Wextra -Werror -std=c++98`.

