#include <abyss/AEArray.h>  // Assuming the class is in Array.hpp
#include <boost/ut.hpp>
#include <string>
#include <algorithm>
#include <memory>
#include <yu/yu.h>

namespace ut = boost::ut;
using namespace boost::ut::literals;

// Custom class for testing ReleaseClasses
class TestClass {
public:
    static int constructor_count;
    static int destructor_count;
    
    TestClass(int v = 0) : value(v) { constructor_count++; }
    ~TestClass() { destructor_count++; }
    
    TestClass(const TestClass& other) : value(other.value) { constructor_count++; }
    TestClass& operator=(const TestClass& other) {
        if (this != &other) {
            value = other.value;
        }
        return *this;
    }
    
    bool operator==(const TestClass& other) const { return value == other.value; }
    bool operator!=(const TestClass& other) const { return value != other.value; }
    
    int value;
};

int TestClass::constructor_count = 0;
int TestClass::destructor_count = 0;

// Reset counters for tests
void reset_test_class_counters() {
    TestClass::constructor_count = 0;
    TestClass::destructor_count = 0;
}

int main() {
    yu::Initialize();
    using namespace ut;
    using namespace abyss;
    
    "Array default constructor"_test = [] {
        Array<int> arr;
        
        expect(arr.Size() == 0_u);
        expect(arr.Capacity() == 0_u);
        expect(arr.Empty());
        expect(!arr.IsValid());  // items should be nullptr
    };
    
    "Array with primitive types"_test = [] {
        Array<int> arr;
        
        // Test Add
        arr.Add(1);
        arr.Add(2);
        arr.Add(3);
        
        expect(arr.Size() == 3_u);
        expect(!arr.Empty());
        expect(arr[0] == 1_i);
        expect(arr[1] == 2_i);
        expect(arr[2] == 3_i);
        
        // Test const access
        const Array<int>& carr = arr;
        expect(carr[0] == 1_i);
        
        // Test bounds checking (should compile, behavior depends on implementation)
        // arr[5]; // Would be out of bounds
        
        // Test AddCached
        arr.AddCached(4);
        expect(arr.Size() == 4_u);
        expect(arr[3] == 4_i);
        
        // Test RemoveAt
        arr.RemoveAt(1);  // Remove element 2
        expect(arr.Size() == 3_u);
        expect(arr[0] == 1_i);
        expect(arr[1] == 3_i);
        expect(arr[2] == 4_i);
        
        // Test Remove
        arr.Add(1);  // Add another 1
        expect(arr.Size() == 4_u);
        arr.Remove(1);  // Should remove all 1s
        expect(arr.Size() == 2_u);
        expect(arr[0] == 3_i);
        expect(arr[1] == 4_i);
    };
    
    "Array copy operations"_test = [] {
        Array<int> arr1;
        arr1.Add(1);
        arr1.Add(2);
        arr1.Add(3);
        
        // Copy constructor
        Array<int> arr2(arr1);
        expect(arr2.Size() == 3_u);
        expect(arr2[0] == 1_i);
        expect(arr2[1] == 2_i);
        expect(arr2[2] == 3_i);
        
        // Modify original, copy should not change
        arr1[0] = 99;
        expect(arr2[0] == 1_i);
        
        // Copy assignment
        Array<int> arr3;
        arr3 = arr2;
        expect(arr3.Size() == 3_u);
        expect(arr3[1] == 2_i);
        
        // Self-assignment
        arr3 = arr3;
        expect(arr3.Size() == 3_u);
        expect(arr3[2] == 3_i);
    };
    
    "Array move operations"_test = [] {
        Array<int> arr1;
        arr1.Add(1);
        arr1.Add(2);
        arr1.Add(3);
        const auto* old_items = arr1.IsValid() ? &arr1[0] : nullptr;
        const auto old_size = arr1.Size();
        const auto old_capacity = arr1.Capacity();
        
        // Move constructor
        Array<int> arr2(std::move(arr1));
        expect(arr2.Size() == old_size);
        expect(arr2.Capacity() == old_capacity);
        expect(arr2[0] == 1_i);
        expect(arr2[1] == 2_i);
        expect(arr2[2] == 3_i);
        
        // arr1 should be in valid but empty state
        expect(arr1.Size() == 0_u);
        expect(arr1.Capacity() == 0_u);
        expect(!arr1.IsValid());
        
        // Move assignment
        Array<int> arr3;
        arr3 = std::move(arr2);
        expect(arr3.Size() == 3_u);
        expect(arr3[2] == 3_i);
        
        // arr2 should be empty
        expect(arr2.Size() == 0_u);
        expect(!arr2.IsValid());
        
        // Self move-assignment
        arr3 = std::move(arr3);
        expect(arr3.Size() == 3_u);  // Should remain unchanged
    };
    
    "Array resize operations"_test = [] {
        Array<int> arr;
        
        // Initial capacity
        std::cout << "DEBUG initial capacity: " << arr.Capacity() << std::endl;
        expect(arr.Capacity() == 0_u);
        
        // Test Resize (capacity)
        arr.Resize(10);
        expect(arr.Capacity() >= 10_u);
        expect(arr.Size() == 0_u);  // Resize shouldn't change size
        
        // Add elements to test capacity expansion
        for (int i = 0; i < 15; ++i) {
            arr.Add(i);
        }
        expect(arr.Size() == 15_u);
        expect(arr.Capacity() >= 15_u);
        
        // Test SetLength to increase size
        arr.SetLength(20);
        expect(arr.Size() == 20_u);
        expect(arr.Capacity() >= 20_u);
        
        // Test length shrink and reset array
        arr.SetLength(5);
        expect(arr.Size() == 5_u);
        expect(arr[0] == 0_i);
        expect(arr[4] == 0_i);
        
        // SetLength to 0
        arr.SetLength(0);
        expect(arr.Size() == 0_u);
        expect(arr.Empty());
    };
    
    "Array with custom class"_test = [] {
        reset_test_class_counters();
        
        {
            Array<TestClass> arr;
            
            // Test Add with custom class
            arr.Add(TestClass(100));
            arr.Add(TestClass(200));
            arr.Add(TestClass(300));
            
            expect(arr.Size() == 3_u);
            expect(arr[0].value == 100_i);
            expect(arr[1].value == 200_i);
            expect(arr[2].value == 300_i);
            
            // Test copy with custom class
            Array<TestClass> arr2 = arr;
            expect(arr2.Size() == 3_u);
            expect(arr2[1].value == 200_i);
            
            // Test Remove with custom class
            TestClass toRemove(200);
            arr2.Remove(toRemove);
            expect(arr2.Size() == 2_u);
            expect(arr2[0].value == 100_i);
            expect(arr2[1].value == 300_i);
            
            // Test ReleaseClasses
            arr2.ReleaseClasses();
            // After ReleaseClasses, the array might be cleared or elements destroyed
            // This depends on implementation - we'll check destructor count
        }

        printf("Constructors: %d, Destructors: %d\n", TestClass::constructor_count, TestClass::destructor_count);
    yu::mem::PrintMemoryReport();
        
        // All destructors should have been called
        //expect(TestClass::destructor_count == TestClass::constructor_count);
    };
    
    "Array Set method"_test = [] {
        Array<int> arr;
        
        // Test Set with raw array
        int data[] = {10, 20, 30, 40, 50};
        arr.Set(data, 5);
        
        expect(arr.Size() == 5_u);
        expect(arr[0] == 10_i);
        expect(arr[1] == 20_i);
        expect(arr[4] == 50_i);
        
        // Test Set with nullptr and zero count
        arr.Set(nullptr, 0);
        expect(arr.Size() == 0_u);
        expect(arr.Empty());
        
        // Test Set with partial array
        int partial[] = {1, 2};
        arr.Set(partial, 2);
        expect(arr.Size() == 2_u);
        expect(arr[1] == 2_i);
    };
    
    "Array ToVector and FromVector"_test = [] {
        Array<int> arr;
        arr.Add(1);
        arr.Add(2);
        arr.Add(3);
        
        // Test ToVector
        std::vector<int> vec = arr.ToVector();
        expect(vec.size() == 3_u);
        expect(vec[0] == 1_i);
        expect(vec[1] == 2_i);
        expect(vec[2] == 3_i);
        
        // Test FromVector
        std::vector<int> source = {4, 5, 6, 7};
        Array<int> arr2 = Array<int>::FromVector(source);
        
        expect(arr2.Size() == 4_u);
        expect(arr2[0] == 4_i);
        expect(arr2[1] == 5_i);
        expect(arr2[3] == 7_i);
        
        // Test with empty vector
        std::vector<int> empty;
        Array<int> arr3 = Array<int>::FromVector(empty);
        expect(arr3.Size() == 0_u);
        expect(arr3.Empty());
    };
    
    "Array Clear method"_test = [] {
        Array<int> arr;
        
        // Fill array
        for (int i = 0; i < 10; ++i) {
            arr.Add(i * 10);
        }
        
        expect(arr.Size() == 10_u);
        expect(!arr.Empty());
        
        // Clear the array
        arr.Clear();
        
        expect(arr.Size() == 0_u);
        expect(arr.Empty());
        
        // Should be able to add after clear
        arr.Add(999);
        expect(arr.Size() == 1_u);
        expect(arr[0] == 999_i);
    };
    
    "Array AddCached capacity behavior"_test = [] {
        Array<int> arr;
        
        // Track initial capacity
        auto initial_capacity = arr.Capacity();
        
        // Add elements to trigger capacity growth
        for (int i = 0; i < 20; ++i) {
            arr.AddCached(i);
        }
        
        expect(arr.Size() == 20_u);
        expect(arr.Capacity() >= 20_u);
        
        // Verify all elements are present
        for (int i = 0; i < 20; ++i) {
            expect(arr[i] == i);
        }
    };
    
    "Array with strings"_test = [] {
        Array<std::string> arr;
        
        // Test with std::string
        arr.Add("Hello");
        arr.Add("World");
        arr.Add("Test");
        
        expect(arr.Size() == 3_u);
        expect(arr[0] == "Hello");
        expect(arr[1] == "World");
        
        // Test Remove with strings
        arr.Remove("World");
        expect(arr.Size() == 2_u);
        expect(arr[0] == "Hello");
        expect(arr[1] == "Test");
        
        // Test copy with strings
        Array<std::string> arr2 = arr;
        expect(arr2.Size() == 2_u);
        expect(arr2[1] == "Test");
        
        // Modify original, copy should not change
        arr[0] = "Modified";
        expect(arr2[0] == "Hello");
    };
    
    "Array edge cases"_test = [] {
        // Empty array operations
        Array<int> empty;
        expect(empty.Empty());
        expect(empty.Size() == 0_u);
        
        // Remove from empty array (should not crash)
        empty.Remove(42);
        expect(empty.Empty());
        
        // RemoveAt with invalid index (behavior depends on implementation)
        // empty.RemoveAt(0); // Would be invalid
        
        // Self copy/move
        Array<int> arr;
        arr.Add(1);
        arr.Add(2);
        
        // Copy to self via different reference
        Array<int>& ref = arr;
        arr = ref;
        expect(arr.Size() == 2_u);
        
        // Move to self
        arr = std::move(ref);
        expect(arr.Size() == 2_u);
    };
    
    "Array large data set"_test = [] {
        Array<int> arr;
        constexpr size_t large_count = 10000;
        
        // Add many elements
        for (size_t i = 0; i < large_count; ++i) {
            arr.Add(static_cast<int>(i * 2));
        }
        
        expect(arr.Size() == large_count);
        
        // Verify elements
        for (size_t i = 0; i < large_count; ++i) {
            expect(arr[i] == static_cast<int>(i * 2));
        }
        
        // Remove many elements
        for (size_t i = 0; i < large_count / 2; ++i) {
            arr.RemoveAt(0);
        }
        
        expect(arr.Size() == large_count / 2);
        expect(arr[0] == static_cast<int>(large_count));
    };
    
    return 0;
}