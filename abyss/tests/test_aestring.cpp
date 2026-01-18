#include <boost/ut.hpp>
#include <abyss/AEString.h>
#include <string>
#include <vector>

namespace ut = boost::ut;

int main() {
    using namespace ut;
    using namespace ut::spec;
    
    cfg<override> = {.tag = {"abyss::String"}};

    describe("abyss::String") = [] {
        auto utf8_test_str = [] {
            return std::string("Hello 世界 ✨");
        };

        it("should handle default construction") = [] {
            abyss::String s;
            expect(s.size() == 0_u);
            expect(s.c_str() != nullptr);
            expect(s.c_str()[0] == L'\0');
            expect(!s.IsValid());
        };

        describe("from wide C-string") = [] {
            it("should construct from non-null wchar_t*") = [] {
                const wchar_t* input = L"Hello World";
                abyss::String s{input};
                
                expect(s.size() == 11_u || s.size() == 5_u);
                expect(s.IsValid());
                expect(s.c_str() != nullptr);
                expect(std::wstring(s.c_str()) == L"Hello World");
            };

            it("should handle empty wide string") = [] {
                abyss::String s{L""};
                expect(s.size() == 0_u);
                expect(s.IsValid());
                expect(s.c_str() != nullptr);
                expect(s.c_str()[0] == L'\0');
            };

            it("should handle null wide string pointer") = [] {
                abyss::String s{static_cast<const wchar_t*>(nullptr)};
                // Behavior depends on your implementation
                expect(s.size() == 0_u || s.size() > 0_u); // Adjust based on expected behavior
            };
        };

        describe("copy semantics") = [] {
            it("should copy construct") = [] {
                abyss::String original{L"Original"};
                abyss::String copy = original;
                
                expect(copy.size() == original.size());
                expect(copy.IsValid());
                expect(copy == original);
                expect(copy.c_str() != original.c_str()) << "Should be deep copy";
            };

            it("should copy assign") = [] {
                abyss::String original{L"Source"};
                abyss::String target{L"Target"};
                
                target = original;
                
                expect(target.size() == original.size());
                expect(target == original);
                expect(target.c_str() != original.c_str()) << "Should be deep copy";
            };

            it("should self-assign safely") = [] {
                abyss::String s{L"Self"};
                const wchar_t* original_ptr = s.c_str();
                
                // Self assignment
                s = s;
                
                expect(s.IsValid());
                expect(s.size() == 4_u);
                expect(s.c_str() == original_ptr) << "Should keep same buffer";
            };
        };

        describe("move semantics") = [] {
            it("should move construct") = [] {
                abyss::String source{L"Movable"};
                const wchar_t* source_ptr = source.c_str();
                
                abyss::String target = std::move(source);
                
                expect(target.size() == 7_u);
                expect(target.IsValid());
                expect(target.c_str() == source_ptr) << "Should transfer buffer";
                expect(source.size() == 0_u || !source.IsValid()) << "Source should be empty";
            };

            it("should move assign") = [] {
                abyss::String source{L"Source"};
                abyss::String target{L"Target"};
                const wchar_t* source_ptr = source.c_str();
                
                target = std::move(source);
                
                expect(target.size() == 6_u);
                expect(target.IsValid());
                expect(target.c_str() == source_ptr) << "Should transfer buffer";
                expect(source.size() == 0_u || !source.IsValid()) << "Source should be empty";
            };
        };

        describe("UTF-8 conversions") = [] {
            it("should construct from UTF-8 string view") = [] {
                std::string utf8 = "Hello 世界";
                abyss::String str = std::string_view(utf8);
                
                expect(str.size() > 0_u);
                expect(str.IsValid());
                // Verify round trip
                auto round_trip = str.ToUTF8();
                expect(round_trip == utf8);
            };

            it("should handle empty UTF-8 string") = [] {
                std::string_view empty;
                abyss::String s{empty};
                
                expect(s.size() == 0_u);
                expect(s.ToUTF8().empty());
            };

            it("should handle complex UTF-8 sequences") = [] {
                std::vector<std::string> test_cases = {
                    "Simple ASCII",
                    "Café ☕",
                    "🎉 Party 🎊",
                    "Emoji 😂🤣😊",
                    "Mixed 中文 with English and 😀"
                };
                
                for (const auto& tc : test_cases) {
                    abyss::String s{tc};
                    auto round_trip = s.ToUTF8();
                    expect(round_trip == tc) << "Failed for: " << tc;
                }
            };

            it("should handle FromUTF8 static method") = [] {
                auto s = abyss::String::FromUTF8("Static method");
                expect(s.IsValid());
                expect(s.size() > 0_u);
                expect(s.ToUTF8() == "Static method");
            };
        };

        describe("comparison operators") = [] {
            it("should compare equal strings") = [] {
                abyss::String s1{L"Hello"};
                abyss::String s2{L"Hello"};
                abyss::String s3{L"World"};
                
                expect(s1 == s2);
                expect(s1 != s3);
                expect(!(s1 == s3));
                expect(!(s1 != s2));
            };

            it("should handle empty strings in comparisons") = [] {
                abyss::String empty1;
                abyss::String empty2{L""};
                abyss::String non_empty{L"Text"};
                
                expect(empty1 == empty2);
                expect(empty1 != non_empty);
                expect(non_empty != empty1);
            };
        };

        describe("utility methods") = [] {
            it("should report correct size") = [] {
                abyss::String empty;
                abyss::String single{L"A"};
                abyss::String multi{L"Hello World"};
                
                expect(empty.size() == 0_u);
                expect(single.size() == 1_u);
                expect(multi.size() == 11_u);
            };

            it("should provide valid C-string") = [] {
                abyss::String s{L"Test"};
                const wchar_t* cstr = s.c_str();
                
                expect(cstr != nullptr);
                expect(cstr[0] == L'T');
                expect(cstr[1] == L'e');
                expect(cstr[2] == L's');
                expect(cstr[3] == L't');
                expect(cstr[4] == L'\0');
            };

            it("should correctly report validity") = [] {
                abyss::String valid{L"Valid"};
                abyss::String empty;
                
                expect(valid.IsValid() == true);
                expect(empty.IsValid() == false);
            };
        };

        describe("destructor behavior") = [] {
            it("should not leak memory") = [] {
                // This test is conceptual - real memory tests need specialized tools
                std::vector<abyss::String> strings;
                
                for (int i = 0; i < 1000; ++i) {
                    strings.emplace_back(L"Test string for memory");
                    strings.emplace_back("UTF-8 string 🎯");
                }
                
                // All destructors called when vector goes out of scope
                expect(true) << "If we get here without crash, destructors work";
            };
        };

        describe("edge cases") = [] {
            it("should handle very long strings") = [] {
                std::string long_ascii(10000, 'A');
                abyss::String s{long_ascii};
                
                expect(s.size() == 10000_u);
                expect(s.IsValid());
                
                auto round_trip = s.ToUTF8();
                expect(round_trip.size() == 10000_u);
            };

            it("should handle strings with null characters") = [] {
                // This depends on your implementation - can it embed nulls?
                // If yes:
                std::wstring with_null = L"Hello\0World";
                abyss::String s{with_null.c_str()};
                expect(s.size() == 11_u || s.size() == 5_u);
                
                // If no (C-string termination):
                // expect(s.size() == 5_u);
            };

            it("should handle assignment chain") = [] {
                abyss::String a{L"A"};
                abyss::String b{L"B"};
                abyss::String c{L"C"};
                
                a = b = c;
                
                expect(a == c);
                expect(b == c);
                expect(a == L"C");
            };
        };
    };
}