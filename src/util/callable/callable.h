#pragma once

#include <any>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

class CallableCustom;

class Callable {
public:
	struct CallError {
		enum Error {
			CALL_OK,
			CALL_ERROR_INVALID_METHOD,
			CALL_ERROR_INVALID_ARGUMENT,
			CALL_ERROR_TOO_MANY_ARGUMENTS,
			CALL_ERROR_TOO_FEW_ARGUMENTS,
			CALL_ERROR_INSTANCE_IS_NULL,
		};
		Error error = Error::CALL_OK;
		int argument = 0;
		int expected = 0;
	};

private:
	std::string method;
	union {
		uint64_t object = 0;
		CallableCustom *custom;
	};

public:
	void callp(const std::vector<std::any> &p_arguments, CallError &r_call_error) const;

	bool is_null() const {
		return method.empty() && object == 0;
	}
	bool is_custom() const {
		return method.empty() && custom != nullptr;
	}
	bool is_standard() const {
		return !method.empty();
	}
	bool is_valid() const;

	void *get_object() const;
	uint64_t get_object_id() const;
	std::string get_method() const;
	CallableCustom *get_custom() const;

	uint32_t hash() const;

	bool operator==(const Callable &p_callable) const;
	bool operator!=(const Callable &p_callable) const;
	bool operator<(const Callable &p_callable) const;

	void operator=(const Callable &p_callable);

	explicit operator std::string() const;

	static Callable create(void *p_object, const std::string &p_method);
	static Callable create(uint64_t p_object_id, const std::string &p_method);

	Callable(void *p_object, const std::string &p_method);
	Callable(uint64_t p_object_id, const std::string &p_method);
	Callable(CallableCustom *p_custom);
	Callable(const Callable &p_callable);
	Callable() {}
	~Callable();
};

class CallableCustom {
	friend class Callable;
	std::atomic<uint64_t> ref_count{1};
	bool referenced = false;
	static std::atomic<uint64_t> next_id;
	uint64_t id;

public:
	using CompareEqualFunc = bool (*)(const CallableCustom *p_a, const CallableCustom *p_b);
	typedef bool (*CompareLessFunc)(const CallableCustom *p_a, const CallableCustom *p_b);

	virtual uint32_t hash() const = 0;
	virtual std::string get_as_text() const = 0;
	virtual CompareEqualFunc get_compare_equal_func() const = 0;
	virtual CompareLessFunc get_compare_less_func() const = 0;
	virtual bool is_valid() const { return true; }
	virtual std::string get_method() const { return ""; }
	virtual uint64_t get_object() const = 0;
	virtual void call(const std::vector<std::any> &p_arguments, Callable::CallError &r_call_error) const = 0;

	uint64_t get_id() const { return id; }

	CallableCustom();
	virtual ~CallableCustom() {}
};

class CallableCustomFunction : public CallableCustom {
	std::function<void(const std::vector<std::any> &)> func;
	std::string text;

public:
	explicit CallableCustomFunction(std::function<void(const std::vector<std::any> &)> p_func, std::string p_text = "CallableCustomFunction")
		: func(std::move(p_func)), text(std::move(p_text)) {}

	virtual uint32_t hash() const override {
		return static_cast<uint32_t>(get_id());
	}

	virtual std::string get_as_text() const override {
		return text;
	}

	virtual CompareEqualFunc get_compare_equal_func() const override {
		return [](const CallableCustom *p_a, const CallableCustom *p_b) -> bool {
			return p_a->get_id() == p_b->get_id();
		};
	}

	virtual CompareLessFunc get_compare_less_func() const override {
		return [](const CallableCustom *p_a, const CallableCustom *p_b) -> bool {
			return p_a->get_id() < p_b->get_id();
		};
	}

	virtual uint64_t get_object() const override {
		return 0;
	}

	virtual void call(const std::vector<std::any> &p_arguments, Callable::CallError &r_call_error) const override {
		if (func) {
			func(p_arguments);
			r_call_error.error = Callable::CallError::CALL_OK;
		} else {
			r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		}
	}
};

namespace detail {
	template <typename T>
	decltype(auto) any_cast_helper(std::any &a) {
		using U = std::remove_reference_t<T>;
		if constexpr (std::is_lvalue_reference_v<T>) {
			return static_cast<T>(std::any_cast<U &>(a));
		} else if constexpr (std::is_rvalue_reference_v<T>) {
			return static_cast<T>(std::any_cast<U &>(a));
		} else {
			return std::any_cast<U>(a);
		}
	}

	template <typename T>
	decltype(auto) any_cast_helper(const std::any &a) {
		using U = std::remove_reference_t<T>;
		if constexpr (std::is_lvalue_reference_v<T> && std::is_const_v<std::remove_reference_t<T>>) {
			return static_cast<T>(std::any_cast<const U &>(a));
		} else if constexpr (std::is_lvalue_reference_v<T>) {
			return static_cast<T>(std::any_cast<U &>(a));
		} else if constexpr (std::is_rvalue_reference_v<T>) {
			return static_cast<T>(std::any_cast<U &>(a));
		} else {
			return std::any_cast<U>(a);
		}
	}

	template <typename Func, typename... Args, size_t... Is>
	void call_with_any(Func &p_func, const std::vector<std::any> &p_args, std::index_sequence<Is...>) {
		p_func(any_cast_helper<Args>(const_cast<std::any &>(p_args[Is]))...);
	}
}

template <typename... Args>
class Signal {
	std::vector<Callable> connections;

public:
	Callable connect(const Callable &p_callable) {
		connections.push_back(p_callable);
		return p_callable;
	}

	template <typename Func>
	Callable connect(Func &&p_func) {
		auto wrapper = [func = std::forward<Func>(p_func)](const std::vector<std::any> &p_args) {
			detail::call_with_any<Func, Args...>(func, p_args, std::index_sequence_for<Args...>{});
		};
		Callable callable(new CallableCustomFunction(std::move(wrapper)));
		connections.push_back(callable);
		return callable;
	}

	void disconnect(const Callable &p_callable) {
		auto it = std::find(connections.begin(), connections.end(), p_callable);
		if (it != connections.end()) {
			connections.erase(it);
		}
	}

	bool is_connected(const Callable &p_callable) const {
		return std::find(connections.begin(), connections.end(), p_callable) != connections.end();
	}

	bool has_connections() const {
		return !connections.empty();
	}

	void emit(Args... p_args) {
		if (connections.empty())
			return;
		std::vector<std::any> args = { std::any(std::forward<Args>(p_args))... };
		for (const auto &callable : connections) {
			Callable::CallError err;
			callable.callp(args, err);
		}
	}

	void Broadcast(Args... p_args) {
		emit(std::forward<Args>(p_args)...);
	}

	void clear() {
		connections.clear();
	}

	std::vector<Callable> get_connections() const {
		return connections;
	}
};
