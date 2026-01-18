#include <string>
#include <string_view>
#include <cstdint>
#include <vector>

namespace abyss
{

    /**
     * @brief AEString - A simple string structure used in Abyss engine
     *
     */
    class String
    {
        wchar_t *text;
        std::uint32_t length;

    public:
        /**
         * @brief Construct a new String object
         *
         */
        String();
        /**
         * @brief Construct a new String object from a wide C-string
         *
         * @param str Wide character string
         */
        String(const wchar_t *str);
        /**
         * @brief Construct a new String object by copying another String
         *
         * @param other Another String object
         */
        String(const String &other);
        /**
         * @brief Construct a new String object by moving another String
         *
         * @param other Another String object to move
         */
        String(String &&other) noexcept;
        /**
         * @brief Copy assignment operator
         *
         * @param other Another String object
         * @return String& Reference to this String object
         */
        String &operator=(const String &other);
        /**
         * @brief Move assignment operator
         *
         * @param other Another String object to move
         * @return String& Reference to this String object
         */
        String &operator=(String &&other) noexcept;
        /**
         * @brief Construct a new String object from a UTF-8 string view
         *
         * @param sv UTF-8 string view
         */
        String(std::string_view sv);
        /**
         * @brief Destroy the String object
         *
         */
        ~String();

        /**
         * @brief Get the wide C-string representation
         *
         * @return const wchar_t* Wide character string
         */
        const wchar_t *c_str() const;
        /**
         * @brief Get the length of the string
         *
         * @return std::uint32_t Length of the string
         */
        std::uint32_t size() const;

        /**
         * @brief Equality operator
         *
         * @param other Another String object
         * @return true If strings are equal
         * @return false If strings are not equal
         */
        bool operator==(const String &other) const;
        /**
         * @brief Inequality operator
         *
         * @param other Another String object
         * @return true If strings are not equal
         * @return false If strings are equal
         */
        bool operator!=(const String &other) const;

        /**
         * @brief Create a String from a UTF-8 string view
         *
         * @param utf8Str UTF-8 string view
         * @return String New String object
         */
        static String FromUTF8(std::string_view utf8Str);
        /**
         * @brief Convert the String to a UTF-8 std::string
         *
         * @return std::string UTF-8 encoded string
         */
        std::string ToUTF8() const;

        /**
         * @brief Check if the String is valid (non-null)
         *
         * @return true If valid
         * @return false If invalid
         */
        bool IsValid() const;
    };

}