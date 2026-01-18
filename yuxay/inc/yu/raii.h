/**
 * @file raii.h
 * @brief RAII helpers and utilities for resource management
 * 
 * Features:
 * - Scope guards for cleanup actions
 * - Defer macro (Go-style)
 * - Non-copyable/non-movable base classes
 * - Resource handle wrappers
 * - Optional/nullable references
 * - Function result wrappers
 */

#pragma once

#include <utility>
#include <functional>
#include <type_traits>
#include <optional>
#include <concepts>
#include <memory>

namespace yu {

// ============================================================================
// Non-copyable / Non-movable Base Classes
// ============================================================================

/// Inherit from this to make a class non-copyable but movable
class NonCopyable {
protected:
    NonCopyable() = default;
    ~NonCopyable() = default;
    
    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(NonCopyable&&) = default;
    
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

/// Inherit from this to make a class non-copyable and non-movable
class NonCopyableNonMovable {
protected:
    NonCopyableNonMovable() = default;
    ~NonCopyableNonMovable() = default;
    
    NonCopyableNonMovable(const NonCopyableNonMovable&) = delete;
    NonCopyableNonMovable& operator=(const NonCopyableNonMovable&) = delete;
    NonCopyableNonMovable(NonCopyableNonMovable&&) = delete;
    NonCopyableNonMovable& operator=(NonCopyableNonMovable&&) = delete;
};

// ============================================================================
// Scope Guard
// ============================================================================

/// Executes a callable when going out of scope (RAII cleanup)
template<std::invocable Func>
class ScopeGuard {
public:
    /// Construct with cleanup function
    explicit ScopeGuard(Func&& func) noexcept(std::is_nothrow_move_constructible_v<Func>)
        : m_func(std::move(func)), m_active(true) {}
    
    /// Move constructor
    ScopeGuard(ScopeGuard&& other) noexcept(std::is_nothrow_move_constructible_v<Func>)
        : m_func(std::move(other.m_func)), m_active(other.m_active) {
        other.m_active = false;
    }
    
    /// Destructor - executes the cleanup function if active
    ~ScopeGuard() noexcept {
        if (m_active) {
            try {
                m_func();
            } catch (...) {
                // Swallow exceptions in destructor
            }
        }
    }
    
    /// Dismiss the guard (prevent cleanup from running)
    void Dismiss() noexcept { m_active = false; }
    
    /// Check if guard is active
    [[nodiscard]] bool IsActive() const noexcept { return m_active; }
    
    // Non-copyable
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard& operator=(ScopeGuard&&) = delete;

private:
    Func m_func;
    bool m_active;
};

/// Create a scope guard from a callable
template<std::invocable Func>
[[nodiscard]] auto MakeScopeGuard(Func&& func) {
    return ScopeGuard<std::decay_t<Func>>(std::forward<Func>(func));
}

// ============================================================================
// Scope Exit / Scope Success / Scope Fail Guards
// ============================================================================

/// Executes on scope exit (always)
template<std::invocable Func>
using ScopeExit = ScopeGuard<Func>;

/// Executes only on successful scope exit (no exception)
template<std::invocable Func>
class ScopeSuccess {
public:
    explicit ScopeSuccess(Func&& func) noexcept(std::is_nothrow_move_constructible_v<Func>)
        : m_func(std::move(func)), m_exceptionCount(std::uncaught_exceptions()) {}
    
    ~ScopeSuccess() noexcept(noexcept(m_func())) {
        if (std::uncaught_exceptions() <= m_exceptionCount) {
            m_func();
        }
    }
    
    ScopeSuccess(const ScopeSuccess&) = delete;
    ScopeSuccess& operator=(const ScopeSuccess&) = delete;
    ScopeSuccess(ScopeSuccess&&) = delete;
    ScopeSuccess& operator=(ScopeSuccess&&) = delete;

private:
    Func m_func;
    int m_exceptionCount;
};

/// Executes only on scope exit due to exception
template<std::invocable Func>
class ScopeFail {
public:
    explicit ScopeFail(Func&& func) noexcept(std::is_nothrow_move_constructible_v<Func>)
        : m_func(std::move(func)), m_exceptionCount(std::uncaught_exceptions()) {}
    
    ~ScopeFail() noexcept {
        if (std::uncaught_exceptions() > m_exceptionCount) {
            try {
                m_func();
            } catch (...) {
                // Swallow exceptions
            }
        }
    }
    
    ScopeFail(const ScopeFail&) = delete;
    ScopeFail& operator=(const ScopeFail&) = delete;
    ScopeFail(ScopeFail&&) = delete;
    ScopeFail& operator=(ScopeFail&&) = delete;

private:
    Func m_func;
    int m_exceptionCount;
};

/// Create a scope success guard
template<std::invocable Func>
[[nodiscard]] auto MakeScopeSuccess(Func&& func) {
    return ScopeSuccess<std::decay_t<Func>>(std::forward<Func>(func));
}

/// Create a scope fail guard
template<std::invocable Func>
[[nodiscard]] auto MakeScopeFail(Func&& func) {
    return ScopeFail<std::decay_t<Func>>(std::forward<Func>(func));
}

// ============================================================================
// Defer Macro (Go-style defer)
// ============================================================================

namespace detail {
    struct DeferHelper {
        template<std::invocable Func>
        auto operator<<(Func&& func) {
            return ScopeGuard<std::decay_t<Func>>(std::forward<Func>(func));
        }
    };
}

/// Defer execution until scope exit
/// Usage: YU_DEFER { cleanup_code(); };
#define YU_DEFER auto YU_CONCAT(_defer_, __LINE__) = ::yu::detail::DeferHelper{} << [&]()

/// Helper macro for unique variable names
#define YU_CONCAT_IMPL(a, b) a##b
#define YU_CONCAT(a, b) YU_CONCAT_IMPL(a, b)

// ============================================================================
// Resource Handle Wrapper
// ============================================================================

/// Traits for resource handles (specialize for your resource types)
template<typename T>
struct HandleTraits {
    static constexpr T InvalidValue() noexcept { return T{}; }
    static void Close(T) noexcept {}
};

/// Generic RAII wrapper for handles/resources
template<typename T, typename Traits = HandleTraits<T>>
class Handle {
public:
    /// Default constructor (invalid handle)
    Handle() noexcept : m_handle(Traits::InvalidValue()) {}
    
    /// Construct from raw handle (takes ownership)
    explicit Handle(T handle) noexcept : m_handle(handle) {}
    
    /// Move constructor
    Handle(Handle&& other) noexcept : m_handle(other.Release()) {}
    
    /// Move assignment
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            Reset(other.Release());
        }
        return *this;
    }
    
    /// Destructor
    ~Handle() noexcept {
        if (m_handle != Traits::InvalidValue()) {
            Traits::Close(m_handle);
        }
    }
    
    /// Get the raw handle
    [[nodiscard]] T Get() const noexcept { return m_handle; }
    
    /// Check if handle is valid
    [[nodiscard]] bool IsValid() const noexcept { 
        return m_handle != Traits::InvalidValue(); 
    }
    
    /// Implicit bool conversion
    [[nodiscard]] explicit operator bool() const noexcept { return IsValid(); }
    
    /// Release ownership and return the handle
    [[nodiscard]] T Release() noexcept {
        T tmp = m_handle;
        m_handle = Traits::InvalidValue();
        return tmp;
    }
    
    /// Reset the handle (closes current and takes new)
    void Reset(T handle = Traits::InvalidValue()) noexcept {
        if (m_handle != Traits::InvalidValue()) {
            Traits::Close(m_handle);
        }
        m_handle = handle;
    }
    
    /// Get address of handle (for output parameters)
    [[nodiscard]] T* GetAddressOf() noexcept {
        Reset();  // Release current handle first
        return &m_handle;
    }
    
    // Non-copyable
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

private:
    T m_handle;
};

// ============================================================================
// Optional Reference (like std::optional but for references)
// ============================================================================

/// Optional reference wrapper
template<typename T>
class OptionalRef {
public:
    OptionalRef() noexcept : m_ptr(nullptr) {}
    OptionalRef(T& ref) noexcept : m_ptr(&ref) {}
    OptionalRef(std::nullopt_t) noexcept : m_ptr(nullptr) {}
    
    [[nodiscard]] bool HasValue() const noexcept { return m_ptr != nullptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return HasValue(); }
    
    [[nodiscard]] T& Value() const {
        if (!m_ptr) throw std::bad_optional_access();
        return *m_ptr;
    }
    
    [[nodiscard]] T& operator*() const { return Value(); }
    [[nodiscard]] T* operator->() const { return m_ptr; }
    
    [[nodiscard]] T& ValueOr(T& defaultValue) const noexcept {
        return m_ptr ? *m_ptr : defaultValue;
    }
    
    void Reset() noexcept { m_ptr = nullptr; }

private:
    T* m_ptr;
};

// ============================================================================
// Finally Block (alternative syntax)
// ============================================================================

/// Use with lambdas for cleanup blocks
template<std::invocable Func>
class Finally {
public:
    explicit Finally(Func func) : m_func(std::move(func)) {}
    ~Finally() { m_func(); }
    
    Finally(const Finally&) = delete;
    Finally& operator=(const Finally&) = delete;
    Finally(Finally&&) = delete;
    Finally& operator=(Finally&&) = delete;

private:
    Func m_func;
};

/// Helper to create Finally from lambda
template<std::invocable Func>
Finally<Func> MakeFinally(Func&& func) {
    return Finally<Func>(std::forward<Func>(func));
}

// ============================================================================
// Lazy Initialization Wrapper
// ============================================================================

/// Thread-safe lazy initialization wrapper
template<typename T>
class Lazy {
public:
    template<typename... Args>
    Lazy(std::in_place_t, Args&&... args)
        : m_factory([args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            return std::apply([](auto&&... a) { return T(std::forward<decltype(a)>(a)...); }, 
                              std::move(args));
        }) {}
    
    template<std::invocable<> Factory>
    explicit Lazy(Factory&& factory) : m_factory(std::forward<Factory>(factory)) {}
    
    /// Get or create the value
    T& Get() {
        std::call_once(m_flag, [this] {
            m_value = m_factory();
        });
        return m_value;
    }
    
    T& operator*() { return Get(); }
    T* operator->() { return &Get(); }

private:
    std::function<T()> m_factory;
    T m_value{};
    std::once_flag m_flag;
};

// ============================================================================
// Cleanup Stack
// ============================================================================

/// Stack of cleanup actions (executed in reverse order)
class CleanupStack : NonCopyableNonMovable {
public:
    CleanupStack() = default;
    
    ~CleanupStack() noexcept {
        ExecuteAll();
    }
    
    /// Push a cleanup action onto the stack
    template<std::invocable Func>
    void Push(Func&& func) {
        m_actions.emplace_back(std::forward<Func>(func));
    }
    
    /// Pop and discard the last cleanup action
    void Pop() {
        if (!m_actions.empty()) {
            m_actions.pop_back();
        }
    }
    
    /// Execute and remove all cleanup actions
    void ExecuteAll() noexcept {
        while (!m_actions.empty()) {
            try {
                m_actions.back()();
            } catch (...) {
                // Swallow exceptions
            }
            m_actions.pop_back();
        }
    }
    
    /// Check if stack is empty
    [[nodiscard]] bool IsEmpty() const noexcept { return m_actions.empty(); }
    
    /// Get number of pending cleanup actions
    [[nodiscard]] std::size_t Size() const noexcept { return m_actions.size(); }

private:
    std::vector<std::function<void()>> m_actions;
};

} // namespace yu
