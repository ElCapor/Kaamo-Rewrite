#include <cstdint>
#include <vector>
#include <yu/memory.h>
#include <type_traits>
#include <cstring>
#include <utility>

constexpr yu::mem::TagId AEArrayTag = yu::mem::Tags::UserStart + 2;

namespace abyss::detail
{
    // Helper to check if a type is trivially relocatable (can use memcpy/memmove)
    template <typename T>
    constexpr bool is_trivially_relocatable_v = std::is_trivially_copyable_v<T>;
}
namespace abyss
{
    /**
     * @brief AEArray - A simple array structure used in Abyss engine
     *
     * @tparam T
     */
    template <typename T>
    class Array
    {
        /**
         * @brief The number of elements currently stored in the array
         *
         */
        std::uint32_t count;
        /**
         * @brief Pointer to the array elements
         *
         */
        T *items;
        /**
         * @brief The total capacity of the array
         *
         */
        std::uint32_t capacity;

    public:
        /**
         * @brief Construct a new Array object
         *
         */
        Array();
        /**
         * @brief Destroy the Array object
         *
         */
        ~Array();
        /**
         * @brief Copy constructor
         *
         * @param other
         */
        Array(const Array &other);
        /**
         * @brief Copy assignment operator
         *
         * @param other
         * @return Array&
         */
        Array &operator=(const Array &other);
        /**
         * @brief Move constructor
         *
         * @param other
         */
        Array(Array &&other) noexcept;
        /**
         * @brief Move assignment operator
         *
         * @param other
         * @return Array&
         */
        Array &operator=(Array &&other) noexcept;
        /**
         * @brief Access operator
         *
         * @param index
         * @return T&
         */
        T &operator[](std::uint32_t index);
        /**
         * @brief Const access operator
         *
         * @param index
         * @return const T&
         */
        const T &operator[](std::uint32_t index) const;

        /**
         * @brief Clears the array and sets its length to newLength
         *
         * @param newLength
         */
        void SetLength(std::uint32_t newLength);
        /**
         * @brief Adds an item to the end of the array
         *
         * @param item
         */
        void Add(const T &item);
        /**
         * @brief Adds an item to the end of the array, doubling capacity if needed
         *
         * @param item
         */
        void AddCached(const T &item);
        /**
         * @brief Removes the item at the specified index
         *
         * @param index
         */
        void RemoveAt(std::uint32_t index);
        /**
         * @brief Removes all occurrences of the specified item
         *
         * @param item
         */
        void Remove(const T &item);
        /**
         * @brief Releases memory for class-type elements
         *
         */
        void ReleaseClasses();
        /**
         * @brief Sets the array contents from a source pointer and count
         *
         * @param src
         * @param newCount
         */
        void Set(T *src, std::uint32_t newCount);
        /**
         * @brief Resizes the array to the new capacity
         *
         * @param newCapacity
         */
        void Resize(std::uint32_t newCapacity);

        /**
         * @brief Gets the number of elements currently stored in the array
         *
         * @return std::uint32_t
         */
        std::uint32_t Size() const;
        /**
         * @brief Gets the total capacity of the array
         *
         * @return std::uint32_t
         */
        std::uint32_t Capacity() const;

        /**
         * @brief Converts the array to a std::vector
         *
         * @return std::vector<T>
         */
        std::vector<T> ToVector() const;
        /**
         * @brief Creates an Array from a std::vector
         *
         * @param vec
         * @return Array<T>
         */
        static Array<T> FromVector(const std::vector<T> &vec);
        /**
         * @brief Clears the array, removing all elements
         *
         */
        void Clear();

        /**
         * @brief Checks if the array is empty
         *
         * @return true
         * @return false
         */
        bool Empty() const;
        /**
         * @brief Checks if the array is valid (non-null items)
         *
         * @return true
         * @return false
         */
        bool IsValid() const;
    };

    template <typename T>
    Array<T>::Array()
        : count(0), items(nullptr), capacity(0) {}

    template <typename T>
    Array<T>::~Array()
    {
        if (items)
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (std::uint32_t i = 0; i < count; ++i)
                {
                    items[i].~T();
                }
            }
            yu::mem::Free(items);
            items = nullptr;
        }
    }

    template <typename T>
    Array<T>::Array(const Array &other)
        : count(other.count), capacity(other.capacity), items(nullptr)
    {
        if (capacity > 0)
        {
            items = static_cast<T *>(yu::mem::Allocate(sizeof(T) * capacity, AEArrayTag));
            if constexpr (detail::is_trivially_relocatable_v<T>)
            {
                std::memcpy(items, other.items, sizeof(T) * count);
            }
            else
            {
                for (std::uint32_t i = 0; i < count; ++i)
                {
                    new (&items[i]) T(other.items[i]);
                }
            }
        }
    }

    template <typename T>
    Array<T> &Array<T>::operator=(const Array &other)
    {
        if (this != &other)
        {
            // Destroy existing elements
            if (items)
            {
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    for (std::uint32_t i = 0; i < count; ++i)
                    {
                        items[i].~T();
                    }
                }
                yu::mem::Free(items);
                items = nullptr;
            }

            count = other.count;
            capacity = other.capacity;

            if (capacity > 0)
            {
                items = static_cast<T *>(yu::mem::Allocate(sizeof(T) * capacity, AEArrayTag));
                if constexpr (detail::is_trivially_relocatable_v<T>)
                {
                    std::memcpy(items, other.items, sizeof(T) * count);
                }
                else
                {
                    for (std::uint32_t i = 0; i < count; ++i)
                    {
                        new (&items[i]) T(other.items[i]);
                    }
                }
            }
        }
        return *this;
    }

    template <typename T>
    Array<T>::Array(Array &&other) noexcept
        : count(other.count), items(other.items), capacity(other.capacity)
    {
        other.count = 0;
        other.items = nullptr;
        other.capacity = 0;
    }

    template <typename T>
    Array<T> &Array<T>::operator=(Array &&other) noexcept
    {
        if (this != &other)
        {
            // Destroy existing elements
            if (items)
            {
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    for (std::uint32_t i = 0; i < count; ++i)
                    {
                        items[i].~T();
                    }
                }
                yu::mem::Free(items);
            }

            count = other.count;
            items = other.items;
            capacity = other.capacity;
            other.count = 0;
            other.items = nullptr;
            other.capacity = 0;
        }
        return *this;
    }

    template <typename T>
    T &Array<T>::operator[](std::uint32_t index)
    {
        return items[index];
    }

    template <typename T>
    const T &Array<T>::operator[](std::uint32_t index) const
    {
        return items[index];
    }

    template <typename T>
    void Array<T>::SetLength(std::uint32_t newLength)
    {
        if constexpr (detail::is_trivially_relocatable_v<T>)
        {
            // POD/trivial types: use memset (matches pseudocode)
            if (capacity == newLength && items != nullptr)
            {
                std::memset(items, 0, sizeof(T) * capacity);
            }
            else
            {
                std::uint32_t newCapacity = (newLength == 0) ? 1 : newLength;
                capacity = newCapacity;
                items = static_cast<T *>(yu::mem::Reallocate(items, sizeof(T) * newCapacity, AEArrayTag));
                std::memset(items, 0, sizeof(T) * newCapacity);
            }
            count = newLength;
        }
        else
        {
            // Non-trivial types: proper construction/destruction
            // Destroy existing elements
            if (items)
            {
                for (std::uint32_t i = 0; i < count; ++i)
                {
                    items[i].~T();
                }
            }

            std::uint32_t newCapacity = (newLength == 0) ? 1 : newLength;
            if (capacity != newCapacity)
            {
                if (items)
                {
                    yu::mem::Free(items);
                }
                items = static_cast<T *>(yu::mem::Allocate(sizeof(T) * newCapacity, AEArrayTag));
                capacity = newCapacity;
            }

            // Default-construct new elements
            for (std::uint32_t i = 0; i < newLength; ++i)
            {
                new (&items[i]) T();
            }
            count = newLength;
        }
    }

    template <typename T>
    void Array<T>::Resize(std::uint32_t newCapacity)
    {
        if (newCapacity == capacity)
            return;

        if constexpr (detail::is_trivially_relocatable_v<T>)
        {
            // POD types: simple realloc
            items = static_cast<T *>(yu::mem::Reallocate(items, sizeof(T) * newCapacity, AEArrayTag));
            if (newCapacity > capacity)
            {
                std::memset(items + capacity, 0, sizeof(T) * (newCapacity - capacity));
            }
            capacity = newCapacity;
            if (count > capacity)
            {
                count = capacity;
            }
        }
        else
        {
            // Non-trivial types: allocate new, move, destroy old
            T *newItems = static_cast<T *>(yu::mem::Allocate(sizeof(T) * newCapacity, AEArrayTag));
            std::uint32_t toCopy = (count < newCapacity) ? count : newCapacity;

            for (std::uint32_t i = 0; i < toCopy; ++i)
            {
                new (&newItems[i]) T(std::move(items[i]));
            }

            // Destroy old elements
            if (items)
            {
                for (std::uint32_t i = 0; i < count; ++i)
                {
                    items[i].~T();
                }
                yu::mem::Free(items);
            }

            items = newItems;
            capacity = newCapacity;
            if (count > capacity)
            {
                count = capacity;
            }
        }
    }

    template <typename T>
    void Array<T>::Add(const T &item)
    {
        // Capacity increases by exactly 1 each time (matches pseudocode)
        std::uint32_t newCapacity = count + 1;

        if constexpr (detail::is_trivially_relocatable_v<T>)
        {
            capacity = newCapacity;
            items = static_cast<T *>(yu::mem::Reallocate(items, sizeof(T) * newCapacity, AEArrayTag));
            items[count] = item;
        }
        else
        {
            // Non-trivial: need proper move/copy
            T *newItems = static_cast<T *>(yu::mem::Allocate(sizeof(T) * newCapacity, AEArrayTag));

            for (std::uint32_t i = 0; i < count; ++i)
            {
                new (&newItems[i]) T(std::move(items[i]));
            }
            new (&newItems[count]) T(item);

            if (items)
            {
                for (std::uint32_t i = 0; i < count; ++i)
                {
                    items[i].~T();
                }
                yu::mem::Free(items);
            }

            items = newItems;
            capacity = newCapacity;
        }
        count = newCapacity;
    }

    template <typename T>
    void Array<T>::AddCached(const T &item)
    {
        if (count >= capacity)
        {
            // Double capacity when full (exponential growth, matches pseudocode)
            std::uint32_t oldCapacity = capacity;
            std::uint32_t newCap = (oldCapacity == 0) ? 1 : (oldCapacity * 2);

            if constexpr (detail::is_trivially_relocatable_v<T>)
            {
                items = static_cast<T *>(yu::mem::Reallocate(items, sizeof(T) * newCap, AEArrayTag));
                // Zero the NEW half only (matches pseudocode)
                std::memset(items + oldCapacity, 0, sizeof(T) * (newCap - oldCapacity));
                capacity = newCap;
            }
            else
            {
                Resize(newCap);
            }
        }

        if constexpr (detail::is_trivially_relocatable_v<T>)
        {
            items[count] = item;
        }
        else
        {
            new (&items[count]) T(item);
        }
        ++count;
    }

    template <typename T>
    void Array<T>::RemoveAt(std::uint32_t index)
    {
        if (count == 0 || index >= count)
            return;

        if constexpr (detail::is_trivially_relocatable_v<T>)
        {
            // Shift elements left
            for (std::uint32_t i = index; i + 1 < count; ++i)
            {
                items[i] = items[i + 1];
            }
            --count;

            // Reallocate to exact size (matches pseudocode Remove behavior)
            std::uint32_t newCapacity = (count == 0) ? 1 : count;
            items = static_cast<T *>(yu::mem::Reallocate(items, sizeof(T) * newCapacity, AEArrayTag));
            if (count == 0)
            {
                std::memset(items, 0, sizeof(T) * newCapacity);
            }
            capacity = newCapacity;
        }
        else
        {
            // Non-trivial: destroy removed element, move remaining
            items[index].~T();

            for (std::uint32_t i = index; i + 1 < count; ++i)
            {
                new (&items[i]) T(std::move(items[i + 1]));
                items[i + 1].~T();
            }
            --count;

            // Shrink to fit
            std::uint32_t newCapacity = (count == 0) ? 1 : count;
            if (newCapacity != capacity)
            {
                T *newItems = static_cast<T *>(yu::mem::Allocate(sizeof(T) * newCapacity, AEArrayTag));
                for (std::uint32_t i = 0; i < count; ++i)
                {
                    new (&newItems[i]) T(std::move(items[i]));
                    items[i].~T();
                }
                yu::mem::Free(items);
                items = newItems;
                capacity = newCapacity;
            }
        }
    }

    template <typename T>
    void Array<T>::Remove(const T &item)
    {
        if (count == 0)
            return;

        if constexpr (detail::is_trivially_relocatable_v<T>)
        {
            // Compact array: shift non-matching elements left (matches pseudocode)
            std::uint32_t newLength = 0;
            for (std::uint32_t i = 0; i < count; ++i)
            {
                if (!(items[i] == item))
                {
                    items[newLength++] = items[i];
                }
            }

            count = newLength;

            // Reallocate to exact size (capacity = length)
            std::uint32_t newCapacity = (count == 0) ? 1 : count;
            items = static_cast<T *>(yu::mem::Reallocate(items, sizeof(T) * newCapacity, AEArrayTag));
            if (count == 0)
            {
                std::memset(items, 0, sizeof(T) * newCapacity);
            }
            capacity = newCapacity;
        }
        else
        {
            // Non-trivial: need to handle destruction properly
            std::uint32_t newLength = 0;
            for (std::uint32_t i = 0; i < count; ++i)
            {
                if (!(items[i] == item))
                {
                    if (newLength != i)
                    {
                        new (&items[newLength]) T(std::move(items[i]));
                        items[i].~T();
                    }
                    ++newLength;
                }
                else
                {
                    items[i].~T();
                }
            }

            count = newLength;

            std::uint32_t newCapacity = (count == 0) ? 1 : count;
            if (newCapacity != capacity)
            {
                T *newItems = static_cast<T *>(yu::mem::Allocate(sizeof(T) * newCapacity, AEArrayTag));
                for (std::uint32_t i = 0; i < count; ++i)
                {
                    new (&newItems[i]) T(std::move(items[i]));
                    items[i].~T();
                }
                yu::mem::Free(items);
                items = newItems;
                capacity = newCapacity;
            }
        }
    }

    template <typename T>
    void Array<T>::ReleaseClasses()
    {
        if constexpr (std::is_pointer_v<T>)
        {
            // For pointers to objects (matches pseudocode)
            using P = std::remove_pointer_t<T>;

            if (items)
            {
                for (std::uint32_t i = 0; i < capacity; ++i)
                {
                    T obj = items[i];
                    if (obj != nullptr)
                    {
                        yu::mem::Delete<P>(obj);
                        items[i] = nullptr;
                    }
                }
                yu::mem::Free(items);
                items = nullptr;
            }
            count = 0;
            capacity = 0;
        }
        else
        {
            // For non-pointer types: just clear the array
            Clear();
        }
    }

    template <typename T>
    void Array<T>::Set(T *src, std::uint32_t newCount)
    {
        std::uint32_t newCapacity = (newCount == 0) ? 1 : newCount;

        if constexpr (detail::is_trivially_relocatable_v<T>)
        {
            // POD types: reallocate and memcpy (matches pseudocode)
            if (capacity != newCapacity)
            {
                capacity = newCapacity;
                items = static_cast<T *>(yu::mem::Reallocate(items, sizeof(T) * newCapacity, AEArrayTag));
            }
            if (newCount > 0 && src != nullptr)
            {
                std::memcpy(items, src, sizeof(T) * newCount);
            }
        }
        else
        {
            // Non-trivial types: destroy old, copy-construct new
            if (items)
            {
                for (std::uint32_t i = 0; i < count; ++i)
                {
                    items[i].~T();
                }
                yu::mem::Free(items);
            }

            items = static_cast<T *>(yu::mem::Allocate(sizeof(T) * newCapacity, AEArrayTag));
            capacity = newCapacity;

            if (newCount > 0 && src != nullptr)
            {
                for (std::uint32_t i = 0; i < newCount; ++i)
                {
                    new (&items[i]) T(src[i]);
                }
            }
        }

        count = newCount;
    }

    template <typename T>
    std::uint32_t Array<T>::Size() const
    {
        return count;
    }

    template <typename T>
    std::uint32_t Array<T>::Capacity() const
    {
        return capacity;
    }

    template <typename T>
    std::vector<T> Array<T>::ToVector() const
    {
        return std::vector<T>(items, items + count);
    }

    template <typename T>
    Array<T> Array<T>::FromVector(const std::vector<T> &vec)
    {
        Array<T> arr;
        if (vec.empty())
        {
            return arr;
        }

        std::uint32_t newCount = static_cast<std::uint32_t>(vec.size());
        std::uint32_t newCapacity = newCount;

        arr.items = static_cast<T *>(yu::mem::Allocate(sizeof(T) * newCapacity, AEArrayTag));
        arr.capacity = newCapacity;
        arr.count = newCount;

        if constexpr (detail::is_trivially_relocatable_v<T>)
        {
            std::memcpy(arr.items, vec.data(), sizeof(T) * newCount);
        }
        else
        {
            for (std::uint32_t i = 0; i < newCount; ++i)
            {
                new (&arr.items[i]) T(vec[i]);
            }
        }

        return arr;
    }

    template <typename T>
    void Array<T>::Clear()
    {
        if (items)
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (std::uint32_t i = 0; i < count; ++i)
                {
                    items[i].~T();
                }
            }
            yu::mem::Free(items);
            items = nullptr;
        }
        count = 0;
        capacity = 0;
    }

    template <typename T>
    bool Array<T>::Empty() const
    {
        return count == 0;
    }

    template <typename T>
    bool Array<T>::IsValid() const
    {
        return items != nullptr;
    }

};