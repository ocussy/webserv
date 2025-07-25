#include "../includes/request.hpp"
#include "../includes/response.hpp"
#include "../includes/config.hpp"
#include <cctype>
#include <cstdlib>
#include <iostream>	

int Request::getPortFromHeaders() const {
    std::map<std::string, std::string>::const_iterator it = _headers.find("Host");
    if (it == _headers.end()) {
        return 8000;
    }
    std::string host = it->second;
    size_t colonPos = host.find(':');
    if (colonPos == std::string::npos) {
        return 8000;
    }
    std::string portStr = host.substr(colonPos + 1);
    int port = std::atoi(portStr.c_str());
    if (port <= 0) {
        return 8000;
    }
    return port;
}


bool Request::isMethodAllowedForRoute(Config &config) {

    std::string url = getUrl();
    size_t lastSlash = url.find_last_of('/');
    if (lastSlash != std::string::npos)
        url = url.substr(0, lastSlash);
    if (url.empty()) {
        url = "/";
    }

    int requestPort = getPortFromHeaders();

    const std::vector<ServerConfig> &servers = config.getServers();

    for (size_t i = 0; i < servers.size(); ++i) {
        const ServerConfig &server = servers[i];
        if (server.getPort() != requestPort) {
            continue;
        }

        const std::vector<LocationConfig>& locations = server.getLocations();

        for (size_t j = 0; j < locations.size(); ++j) {
            const LocationConfig &location = locations[j];
            if (url.find(location.getPath()) == 0 && 
                (url.size() == location.getPath().size() || 
                 url[location.getPath().size()] == '/' || 
                 url[location.getPath().size()] == '?')) {
                _location = location;

                for (size_t k = 0; k < location.getAllowMethod().size(); ++k) {
                    if (_method == location.getAllowMethod()[k]) {
                        return true;
                    }
                }
                return false;
            }
        }
        const std::vector<std::string>& serverAllowedMethods = server.getAllowMethods();
        for (size_t k = 0; k < serverAllowedMethods.size(); ++k) {
            if (_method == serverAllowedMethods[k]) {
                return true;
            }
        }
    }
    return false;
}
bool Request::isValidHttpVersion() {
	return (_httpVersion	== "HTTP/1.1");
}

bool Request::isValidUrl() {
	return !_url.empty()	&& _url[0] == '/' && _url.find("..") == std::string::npos;
}

bool Request::isValidMethod() {
	return (_method == "GET"	|| _method == "POST"	|| _method == "DELETE" ||
			_method == "PUT"	|| _method == "PATCH" || _method == "HEAD" ||
			_method == "OPTIONS"	|| _method == "CONNECT" || _method == "TRACE");
}

size_t Request::safeStringToUlong(const std::string&	str, bool& success) {
	char* end;
	unsigned long result = strtoul(str.c_str(),	&end, 10);
	
	
	if (end	== str.c_str() || *end != '\0')	{
		success	= false;	

		return 0;
	}
	
	success	= true;
	return static_cast<size_t>(result);
}

void Request::parseMultipartFormData(const std::string& body, const std::string& boundary) {
    std::string delimiter = "--" + boundary;
    size_t pos = 0;

    while ((pos = body.find(delimiter, pos)) != std::string::npos) {
        size_t start = pos + delimiter.length();
        if (body.compare(start, 2, "--") == 0)
            break;
        start += 2;
        size_t headersEnd = body.find("\r\n\r\n", start);
        if (headersEnd == std::string::npos) break;

        std::string partHeaders = body.substr(start, headersEnd - start);
        size_t contentStart = headersEnd + 4;
        size_t nextDelimiter = body.find(delimiter, contentStart);

        std::string content = body.substr(contentStart, nextDelimiter - contentStart);

        if (partHeaders.find("filename=") != std::string::npos) {
            size_t fnameStart = partHeaders.find("filename=\"") + 10;
            size_t fnameEnd = partHeaders.find("\"", fnameStart);
            std::string filename = partHeaders.substr(fnameStart, fnameEnd - fnameStart);

            std::ofstream outFile(("uploads/" + filename).c_str(), std::ios::binary);
            outFile.write(content.c_str(), content.size());
            outFile.close();

        }

        pos = nextDelimiter;
    }
}

void Request::parseHeaders(std::istringstream &stream, size_t &headersSize, bool &headersFinished) {
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }
        if (line.empty()) {
            headersFinished = true;
            break;
        }

        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            _errorCode = 400;
            return;
        }

        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);

        if (!key.empty()) {
            size_t lastNonSpace = key.find_last_not_of(" \t\r");
            if (lastNonSpace != std::string::npos)
                key.erase(lastNonSpace + 1);
            else
                key.clear();
        }

        if (!value.empty()) {
            size_t firstNonSpace = value.find_first_not_of(" \t");
            if (firstNonSpace != std::string::npos)
                value.erase(0, firstNonSpace);
            else
                value.clear();
        }

        if (!value.empty() && value[value.size() - 1] == '\r') {
            value.erase(value.size() - 1);
        }

        addHeader(key, value);

        headersSize += line.size();
        if (headersSize > _maxHeadersSize) {
            _errorCode = 431;
            return;
        }
    }
    
}

void Request::parseHostHeader(std::istringstream &stream){
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }
        if (line.find("Host:") == 0) {
            std::string host = line.substr(6);
            host.erase(0, host.find_first_not_of(" \t"));
            host.erase(host.find_last_not_of(" \t") + 1);
            _headers["Host"] = host;
            break;
        }
    }
}

bool Request::validateMethodAndVersion(Config &config) {
    if (!isMethodAllowedForRoute(config)) {
        _errorCode = 405;
        return false;
    }

    if (!isValidHttpVersion()) {
        _errorCode = 505;
        return false;
    }

    return true;
}

void Request::initializeRequest(Request& request, const std::string& method, const std::string& url, const std::string& httpVersion, const std::string& queryString) {
    request.setMethod(method);
    request.setUrl(url);
    request.setHttpVersion(httpVersion);
    request.setQueryString(queryString);
}

void Request::processHeaders(std::istringstream &stream, bool headersFinished) {
    if (!headersFinished) {
        _errorCode = 400;
        return;
    }
    std::map<std::string, std::string>::const_iterator transferEncodingIt = getHeaders().find("Transfer-Encoding");
    if (transferEncodingIt != getHeaders().end() && transferEncodingIt->second == "chunked") {
        processChunkedBody(stream);
        return;
    }
    std::map<std::string, std::string>::const_iterator contentLengthIt = getHeaders().find("Content-Length");
    if (contentLengthIt != getHeaders().end()) {
        bool conversionSuccess = false;
        size_t contentLength = safeStringToUlong(contentLengthIt->second, conversionSuccess);
        if (!conversionSuccess) {
            _errorCode = 400;
            return;
        }

        if (_method == "POST" && contentLength == 0) {
            std::cout << "Content-Length is 0 for POST method.\n";
            _errorCode = 400;
            return;
        }
        if (contentLength > _maxBodySize) {
            _errorCode = 413;
            return;
        }

        if (contentLength > 0) {
            std::string bodyContent;
            bodyContent.reserve(contentLength);

            char *buffer = new char[1024];
            size_t totalRead = 0;
            size_t bytesToRead;
            while (totalRead < contentLength) {
                bytesToRead = (contentLength - totalRead < 1024) ? (contentLength - totalRead) : 1024;
                stream.read(buffer, bytesToRead);
                size_t bytesRead = stream.gcount();
                if (bytesRead == 0) break;

                bodyContent.append(buffer, bytesRead);
                totalRead += bytesRead;
            }

            delete[] buffer;
            setBody(bodyContent);
            if (bodyContent.length() != contentLength) {
                std::cout << "Warning: Body length (" << bodyContent.length()
                          << ") does not match Content-Length (" << contentLength << ")" << std::endl;
                _errorCode = 400;
                return;
            }
        }
    } else if (_method == "POST") {
        _errorCode = 411;
        return;
    }

    if ((_method == "POST") && getHeaders().find("Content-Type") == getHeaders().end()) {
        std::cout << "Content-Type header is missing for POST method.\n";
        _errorCode = 400;
        return;
    }
}

void Request::processChunkedBody(std::istringstream &stream) {
    std::string chunkSizeLine;
    std::string body;
    while (std::getline(stream, chunkSizeLine)) {
        if (!chunkSizeLine.empty() && chunkSizeLine[chunkSizeLine.size() - 1] == '\r') {
            chunkSizeLine.erase(chunkSizeLine.size() - 1);
        }
        size_t chunkSize = std::strtoul(chunkSizeLine.c_str(), NULL, 16);
        if (chunkSize == 0) {
            break;
        }
        std::string chunk(chunkSize, '\0');
        stream.read(&chunk[0], chunkSize);
        body += chunk;
        stream.ignore(2);
    }
    if (body.size() > _maxBodySize) {
        _errorCode = 413;
        return;
    }
    if (body.empty()) return;
    setBody(body);
}

/*  bool conversionSuccess = false;
size_t contentLength = safeStringToUlong(contentLengthIt->second, conversionSuccess);
        
if (!conversionSuccess)	{
    _errorCode = 400;	
    return ;
}
if (_method == "POST" && contentLength == 0) {
    _errorCode = 400; 
    return;
}

if (contentLength >	_maxBodySize) {
    _errorCode = 413;	
    return ;
}
if (contentLength >	0) {
    std::string	bodyContent;
    bodyContent.reserve(contentLength);
    
    char* buffer = new char[1024];
    size_t totalRead = 0;
    size_t bytesToRead;
    while (totalRead < contentLength) {
        bytesToRead	= (contentLength - totalRead < 1024) ? (contentLength -	totalRead) : 1024;
        stream.read(buffer,	bytesToRead);
        size_t bytesRead = stream.gcount();
        //std::cout << "Bytes read : " << bytesRead << std::endl;
        if (bytesRead == 0) break;	
        
        bodyContent.append(buffer, bytesRead);
        //std::cout << "Body content" << bodyContent << std::endl;
        totalRead += bytesRead;
    }
    
    delete[] buffer;
    //std::cout << "Body length : " << bodyContent.length() << std::endl;
    //std::cout << "Content length : " << contentLength << std::endl;
    setBody(bodyContent);
    if (bodyContent.length() != contentLength) {
        std::cout << "Warning: Body	length (" << bodyContent.length() 
                  << ") does not match Content-Length (" << contentLength << ")" << std::endl;
        _errorCode = 400;	
        return;
    }
}
} else if (_method == "POST") {

_errorCode = 411;	
return;
}

if ((_method	== "POST") && 
getHeaders().find("Content-Type") == getHeaders().end()) {
_errorCode = 400;	
return ;
}



std::map<std::string, std::string>::const_iterator contentTypeIt = getHeaders().find("Content-Type");
if (contentTypeIt != getHeaders().end()) {
std::string contentType = contentTypeIt->second;

size_t paramPos = contentType.find(';');
if (paramPos != std::string::npos) {
    contentType = contentType.substr(0, paramPos);
}

size_t lastNonSpace = contentType.find_last_not_of(" \t\r");
if (lastNonSpace != std::string::npos)
    contentType = contentType.substr(0, lastNonSpace + 1);
            
static const std::string allowedTypes[] = {
    "text/html", "image/png", "image/jpeg", "text/css",
    "application/javascript", "application/json", "application/xml",
    "application/pdf", "text/plain", "text/csv", "application/x-www-form-urlencoded", "multipart/form-data"
};

bool isAllowed = false;
for (size_t i = 0; i < sizeof(allowedTypes)/sizeof(allowedTypes[0]); ++i) {
    if (contentType == allowedTypes[i]) {
        isAllowed = true;
        break;
    }
}

if (!isAllowed) {
    _errorCode = 415;
    return;
}
}



std::map<std::string, std::string>::const_iterator expectIt	= getHeaders().find("Expect");
if (expectIt != getHeaders().end())	{
std::string	expect = expectIt->second;
if (expect.find("100-continue")	!= std::string::npos) {
    _errorCode = 417;	
    return ;
}
}
} else {

_errorCode = 400;	
return ;
}*/

