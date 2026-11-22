//
// Created by fang on 2022/5/1.
//

#include "bilibili/util/http.hpp"

#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif
#include <windows.h>
#endif

namespace bilibili {

std::string HTTP::getCABundlePath() {
#ifdef _WIN32
    // 只计算一次：exe 所在目录下的 ca-bundle.crt（便携版）或 resources/ca-bundle.crt
    static const std::string cached = []() -> std::string {
        wchar_t exePath[MAX_PATH] = {0};
        DWORD len                 = ::GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) return {};

        std::filesystem::path dir = std::filesystem::path(exePath).parent_path();
        for (const wchar_t* relative : {L"ca-bundle.crt", L"resources/ca-bundle.crt"}) {
            std::filesystem::path candidate = dir / relative;
            std::error_code ec;
            if (!std::filesystem::is_regular_file(candidate, ec)) continue;

            // libcurl 在 Windows 上按 UTF-8 解析文件路径，这里统一转换
            const std::wstring wide = candidate.wstring();
            int size = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
            if (size <= 0) return {};
            std::string utf8(size, '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &utf8[0], size, nullptr, nullptr);
            return utf8;
        }
        return {};
    }();
    return cached;
#else
    return {};
#endif
}

std::string HTTP::getEncodedCookie(const cpr::Cookies& cookies) {
    std::stringstream stream;
    for (auto it = cookies.begin(); it != cookies.end(); ++it) {
        const cpr::Cookie& item = *it;
        stream << item.GetName() << "=" << item.GetValue();
        if (std::next(it) != cookies.end()) stream << "; ";
    }
    return stream.str();
}

void HTTP::signParameters(cpr::Parameters& parameters) {
    parameters.Add({{"appkey", BILIBILI_APP_KEY},
                    {"build", BILIBILI_BUILD},
                    {"ts", std::to_string(wiliwili::getUnixTime() * 1000)}});
    std::vector<std::string> kv;
    pystring::split(parameters.GetContent(cpr::CurlHolder()), kv, "&");
    std::sort(kv.begin(), kv.end());
    parameters.Add({{"sign", websocketpp::md5::md5_hash_hex(pystring::join("&", kv) + BILIBILI_APP_SECRET)}});
}

};  // namespace bilibili