#include "callable.h"

std::atomic<uint64_t> CallableCustom::next_id{1};

void Callable::callp(const std::vector<std::any> &p_arguments, CallError &r_call_error) const {
	if (is_null()) {
		r_call_error.error = CallError::CALL_ERROR_INSTANCE_IS_NULL;
		r_call_error.argument = 0;
		r_call_error.expected = 0;
	} else if (is_custom()) {
		if (!is_valid()) {
			r_call_error.error = CallError::CALL_ERROR_INSTANCE_IS_NULL;
			r_call_error.argument = 0;
			r_call_error.expected = 0;
			return;
		}
		custom->call(p_arguments, r_call_error);
	} else {
		r_call_error.error = CallError::CALL_ERROR_INVALID_METHOD;
		r_call_error.argument = 0;
		r_call_error.expected = 0;
	}
}

bool Callable::is_valid() const {
	if (is_custom()) {
		return get_custom()->is_valid();
	} else {
		return !method.empty() && object != 0;
	}
}

void *Callable::get_object() const {
	if (is_null()) {
		return nullptr;
	} else if (is_custom()) {
		return reinterpret_cast<void *>(custom->get_object());
	} else {
		return reinterpret_cast<void *>(object);
	}
}

uint64_t Callable::get_object_id() const {
	if (is_null()) {
		return 0;
	} else if (is_custom()) {
		return custom->get_object();
	} else {
		return object;
	}
}

std::string Callable::get_method() const {
	if (is_custom()) {
		return get_custom()->get_method();
	}
	return method;
}

CallableCustom *Callable::get_custom() const {
	if (!is_custom()) {
		return nullptr;
	}
	return custom;
}

uint32_t Callable::hash() const {
	if (is_custom()) {
		return custom->hash();
	} else {
		uint32_t h = 0;
		for (char c : method) {
			h = h * 31 + static_cast<uint32_t>(c);
		}
		h = static_cast<uint32_t>(object ^ (object >> 32));
		return h;
	}
}

bool Callable::operator==(const Callable &p_callable) const {
	bool custom_a = is_custom();
	bool custom_b = p_callable.is_custom();

	if (custom_a == custom_b) {
		if (custom_a) {
			if (custom == p_callable.custom) {
				return true;
			}

			CallableCustom::CompareEqualFunc eq_a = custom->get_compare_equal_func();
			CallableCustom::CompareEqualFunc eq_b = p_callable.custom->get_compare_equal_func();
			if (eq_a == eq_b) {
				return eq_a(custom, p_callable.custom);
			} else {
				return false;
			}
		} else {
			return object == p_callable.object && method == p_callable.method;
		}
	} else {
		return false;
	}
}

bool Callable::operator!=(const Callable &p_callable) const {
	return !(*this == p_callable);
}

bool Callable::operator<(const Callable &p_callable) const {
	bool custom_a = is_custom();
	bool custom_b = p_callable.is_custom();

	if (custom_a == custom_b) {
		if (custom_a) {
			if (custom == p_callable.custom) {
				return false;
			}

			CallableCustom::CompareLessFunc less_a = custom->get_compare_less_func();
			CallableCustom::CompareLessFunc less_b = p_callable.custom->get_compare_less_func();
			if (less_a == less_b) {
				return less_a(custom, p_callable.custom);
			} else {
				return reinterpret_cast<std::uintptr_t>(less_a) < reinterpret_cast<std::uintptr_t>(less_b);
			}
		} else {
			if (object == p_callable.object) {
				return method < p_callable.method;
			} else {
				return object < p_callable.object;
			}
		}
	} else {
		return int(custom_a ? 1 : 0) < int(custom_b ? 1 : 0);
	}
}

void Callable::operator=(const Callable &p_callable) {
	CallableCustom *cleanup_ref = nullptr;
	if (is_custom()) {
		if (p_callable.is_custom()) {
			if (custom == p_callable.custom) {
				return;
			}
		}
		cleanup_ref = custom;
		custom = nullptr;
	}

	if (p_callable.is_custom()) {
		method.clear();
		object = 0;
		if (p_callable.custom->ref_count.fetch_add(1) >= 1) {
			custom = p_callable.custom;
		}
	} else {
		method = p_callable.method;
		object = p_callable.object;
	}

	if (cleanup_ref != nullptr && cleanup_ref->ref_count.fetch_sub(1) == 1) {
		delete cleanup_ref;
	}
	cleanup_ref = nullptr;
}

Callable::operator std::string() const {
	if (is_custom()) {
		return custom->get_as_text();
	} else {
		if (is_null()) {
			return "null::null";
		}
		return std::to_string(object) + "::" + method;
	}
}

Callable Callable::create(void *p_object, const std::string &p_method) {
	if (p_method.empty()) {
		return Callable();
	}
	if (p_object == nullptr) {
		return Callable();
	}
	return Callable(p_object, p_method);
}

Callable Callable::create(uint64_t p_object_id, const std::string &p_method) {
	if (p_method.empty()) {
		return Callable();
	}
	return Callable(p_object_id, p_method);
}

Callable::Callable(void *p_object, const std::string &p_method) {
	if (p_method.empty()) {
		object = 0;
		return;
	}
	if (p_object == nullptr) {
		object = 0;
		return;
	}
	object = reinterpret_cast<uint64_t>(p_object);
	method = p_method;
}

Callable::Callable(uint64_t p_object_id, const std::string &p_method) {
	if (p_method.empty()) {
		object = 0;
		return;
	}
	object = p_object_id;
	method = p_method;
}

Callable::Callable(CallableCustom *p_custom) {
	if (p_custom->referenced) {
		object = 0;
		return;
	}
	p_custom->referenced = true;
	object = 0;
	custom = p_custom;
}

Callable::Callable(const Callable &p_callable) {
	if (p_callable.is_custom()) {
		if (p_callable.custom->ref_count.fetch_add(1) < 1) {
			object = 0;
		} else {
			object = 0;
			custom = p_callable.custom;
		}
	} else {
		method = p_callable.method;
		object = p_callable.object;
	}
}

Callable::~Callable() {
	if (is_custom()) {
		if (custom->ref_count.fetch_sub(1) == 1) {
			delete custom;
			custom = nullptr;
		}
	}
}

CallableCustom::CallableCustom() {
	id = next_id.fetch_add(1);
}
