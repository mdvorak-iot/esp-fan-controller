#pragma once

#include <esp_http_server.h>

class HttpdResponseWriter
{
public:
    HttpdResponseWriter(httpd_req_t *request) : request_(request)
    {
    }

    size_t write(const uint8_t *s, size_t n)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_send_chunk(request_, reinterpret_cast<const char *>(s), n));
        return n;
    }

    size_t write(uint8_t c)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_send_chunk(request_, reinterpret_cast<const char *>(&c), 1));
        return 1;
    }

    esp_err_t close()
    {
        return httpd_resp_send_chunk(request_, nullptr, 0);
    }

private:
    httpd_req_t *request_;
};

class HttpdRequestReader
{

public:
    HttpdRequestReader(httpd_req_t *request) : request_(request)
    {
    }

    int read()
    {
        char c = 0;
        int s = httpd_req_recv(request_, &c, 1);
        if (s == 1)
        {
            return c;
        }
        return -1;
    }

    size_t readBytes(char *buffer, size_t length)
    {
        int s = httpd_req_recv(request_, buffer, length);
        // Negative values are errors
        if (s >= 0)
        {
            return s;
        }
        return 0;
    }

private:
    httpd_req_t *request_;
};
