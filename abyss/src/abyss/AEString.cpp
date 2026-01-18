#include <abyss/AEString.h>
#include <wchar.h>
#include <yu/memory.h>
#include <locale>
#include <cstdlib>
#include <Windows.h>




constexpr yu::mem::TagId AEStringTag = yu::mem::Tags::UserStart + 1;

namespace abyss {
    String::String()
        : text(nullptr), length(0) {}
    
    String::String(const wchar_t* str) {
        if (str) {
            length = static_cast<std::uint32_t>(wcslen(str));
            text = yu::mem::NewArray<wchar_t>(length + 1, AEStringTag);
            std::wcscpy(text, str);
        } else {
            text = nullptr;
            length = 0;
        }
    }

    String::String(const String& other) {
        if (other.text) {
            length = other.length;
            text = yu::mem::NewArray<wchar_t>(length + 1, AEStringTag);
            std::wcscpy(text, other.text);
        } else {
            text = nullptr;
            length = 0;
        }
    }

    String::String(String&& other) noexcept
        : text(other.text), length(other.length) {
        other.text = nullptr;
        other.length = 0;
    }

    String& String::operator=(const String& other) {
        if (this != &other) {
            yu::mem::DeleteArray(text, length + 1);
            if (other.text) {
                length = other.length;
                text = yu::mem::NewArray<wchar_t>(length + 1, AEStringTag);
                std::wcscpy(text, other.text);
            } else {
                text = nullptr;
                length = 0;
            }
        }
        return *this;
    }

    String& String::operator=(String&& other) noexcept {
        if (this != &other) {
            yu::mem::DeleteArray(text, length + 1);
            text = other.text;
            length = other.length;
            other.text = nullptr;
            other.length = 0;
        }
        return *this;
    }

    String::String(std::string_view sv) {
        // Convert UTF-8 string_view to wide string
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, sv.data(), static_cast<int>(sv.size()), nullptr, 0);
        if (wideLen > 0) {
            text = yu::mem::NewArray<wchar_t>(wideLen + 1, AEStringTag);
            MultiByteToWideChar(CP_UTF8, 0, sv.data(), static_cast<int>(sv.size()), text, wideLen);
            text[wideLen] = L'\0';
            length = static_cast<std::uint32_t>(wideLen);
        } else {
            text = nullptr;
            length = 0;
        }
    }

    String::~String() {
        yu::mem::DeleteArray(text, length + 1);
    }

    const wchar_t* String::c_str() const {
        return text ? text : L"";
    }

    std::uint32_t String::size() const {
        return length;
    }

    bool String::operator==(const String& other) const {
        if (length != other.length) return false;
        // Treat empty strings as equal regardless of internal pointer state
        if (length == 0 && other.length == 0) return true;
        if (text == nullptr || other.text == nullptr) return false;
        return std::wcscmp(text, other.text) == 0;
    }

    bool String::operator!=(const String& other) const {
        return !(*this == other);
    }

    String String::FromUTF8(std::string_view utf8Str) {
        return String(utf8Str);
    }

    std::string String::ToUTF8() const {
        if (!text) return std::string();

        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
        if (utf8Len <= 0) return std::string();

        std::string result;
        result.resize(static_cast<std::size_t>(utf8Len));
        WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(length), result.data(), utf8Len, nullptr, nullptr);
        return result;
    }

    bool String::IsValid() const {
        return text != nullptr;
    }
}