#pragma once
#include <common/exception.h>
#include "instruction.h"
#include <script/vm/instructions/instructions.h>
#include "types.h"
#include "function.h"
#include <functional>
#include <script/compiler/compiler.h>
#include <parse/token.h>

namespace script
{
	using VariantPtr = std::shared_ptr<vm::Variant>;
	struct VMContext
	{
		virtual std::string get_string(size_t) = 0;
		virtual int get_int(size_t) = 0;
		virtual vm::ObjectPtr get_object(size_t) = 0;
		virtual float get_float(size_t) = 0;
		virtual void get_vector(size_t, vm::Vector&) = 0;
		virtual VariantPtr get_variant(size_t) = 0;
		virtual std::string variant_to_string(vm::Variant) = 0;
		virtual float variant_to_number(vm::Variant) = 0;
		virtual void add_bool(const bool b)
		{
			add_int(b ? 1 : 0);
		}
		virtual void add_vector(vm::Vector) = 0;
		virtual void add_object(std::shared_ptr<vm::Object>&) = 0;
		virtual void add_int(const int) = 0;
		virtual void add_undefined() = 0;
		virtual void add_float(const float) = 0;
		virtual void add_string(const std::string) = 0;
	};

	using StockFunction = std::function<int(VMContext&, vm::Object*)>;

	namespace vm
	{
		namespace flags
		{
			enum
			{
				kNone = 0,
				kZF = 1
			};
		};

		struct ThreadLock
		{
			virtual bool locked() = 0;
			virtual void notify(const std::string) = 0;
			virtual ~ThreadLock()
			{
			}
		};

		struct ExceptionData
		{
		};
		using Exception = common::TypedDataMessageException<ExceptionData>;
		class VirtualMachine
		{
			int m_flags = flags::kNone;
			compiler::CompiledFiles& m_compiledfiles;
			struct FunctionContext
			{
				std::string file_name;
				std::string function_name;
				VariantPtr self_object;
				std::unordered_map<std::string, std::shared_ptr<vm::Variant>> variables;
				std::unordered_map<size_t, size_t> labels;
				VariantPtr get_variable(const std::string var)
				{
					auto fnd = variables.find(var);
					if (fnd == variables.end())
					{
						variables[var] = std::make_shared<vm::Variant>(vm::Undefined());
					}
					return variables[var];
				}
				size_t instruction_index = 0;
				compiler::CompiledFunction* function = nullptr;
			};
			struct ThreadContext
			{
				std::vector<std::shared_ptr<vm::Variant>> m_stack;
				std::stack<FunctionContext> m_callstack;
				std::vector<std::unique_ptr<ThreadLock>> m_locks;
				FunctionContext& function_context()
				{
					if (m_callstack.empty())
						throw vm::Exception("callstack empty");
					return m_callstack.top();
				}
				bool marked_for_deletion = false;
			};
			std::unordered_map<std::string, StockFunction> m_stockfunctions;
			std::unique_ptr<VMContext> m_context;

			std::vector<std::unique_ptr<ThreadContext>> m_threads;
			std::vector<std::unique_ptr<ThreadContext>> m_newthreads;
			ThreadContext* m_thread;
			std::vector<std::string> event_strings;

			VariantPtr level_object;
			VariantPtr game_object;

			//TODO: FIXME, why is this here?
			//well because when we include something, the preprocessor has a include guard and then it won't get included in this particular file when
			//we're looking for a function
			//hackish solution, just make a large global list of all the functions and then if we can't find the function
			//just try to find it here
			std::unordered_map<std::string, compiler::CompiledFunction*> m_allcustomfunctions;

		  public:
			std::shared_ptr<vm::Instruction> fetch(ThreadContext*);
			FunctionContext& function_context()
			{
				return m_thread->function_context();
			}
			size_t thread_count()
			{
				return m_threads.size();
			}
			void set_flags(int flags)
			{
				m_flags = flags;
			}

			VariantPtr get_level_object()
			{
				return level_object;
			}

			int get_flags()
			{
				return m_flags;
			}

			ThreadContext* thread()
			{
				return m_thread;
			}

			void jump(size_t i)
			{
				auto& fc = function_context();
				auto fnd = fc.labels.find(i);
				if (fnd == fc.labels.end())
				{
					for (auto& it : fc.labels)
					{
						printf("label: %d\n", it.first);
					}
					throw vm::Exception("cannot jump to non existing label {}", i);
				}
				fc.instruction_index = fnd->second;
			}

			VariantPtr get_variable(const std::string var);
			std::string variant_to_string_for_dump(VariantPtr v);
			void dump_object(const std::string, VariantPtr ptr, int indent);
			void dump(ThreadContext*);
			void notify_event_string(const std::string str)
			{
				event_strings.push_back(str);
			}
			std::unique_ptr<VMContext>& context()
			{
				return m_context;
			}
			void push(VariantPtr v)
			{
				m_thread->m_stack.push_back(v);
			}
			compiler::CompiledFunction* find_function_in_file(const std::string file, const std::string function);
			VariantPtr top(int offset = 0)
			{
				auto& stack = m_thread->m_stack;
				if (stack.empty())
					throw vm::Exception("stack empty");
				size_t offs = stack.size() - 1 - offset;
				if (offs >= stack.size())
					throw vm::Exception("out of bounds");
				return stack[offs];
			}
			VariantPtr pop(size_t n = 1)
			{
				VariantPtr v;
				for (size_t i = 0; i < n; ++i)
				{
					if (m_thread->m_stack.empty())
						throw vm::Exception("empty stack");
					v = m_thread->m_stack[m_thread->m_stack.size() - 1];
					m_thread->m_stack.pop_back();
				}
				return v;
			}
			template <typename T> VariantPtr variant(T t)
			{
				return std::make_shared<vm::Variant>(t);
			}
			void register_function(const std::string name, StockFunction sf)
			{
				m_stockfunctions[name] = sf;
			}
			VirtualMachine(compiler::CompiledFiles&);
			void run();
			void call(ThreadContext*, VariantPtr obj, const std::string, const std::string, size_t);
			void call(VariantPtr obj, const std::string, size_t);
			void call_builtin(VariantPtr obj, const std::string, size_t);
			void notify(VariantPtr obj, size_t);
			void waittill(VariantPtr obj, size_t);
			void endon(VariantPtr obj, size_t);
			void ret();

			std::string variant_to_string(vm::Variant v);
			float variant_to_number(vm::Variant v);
			int variant_to_integer(vm::Variant v);
			void exec_thread(VariantPtr obj, const std::string file, const std::string function, size_t numargs);
			void exec_thread(VariantPtr obj, const std::string function, size_t numargs);

			template <typename T> vm::Variant handle_binary_op(const T& a, const T& b, int op)
			{
				switch (op)
				{
				case '-':
					return a - b;
				case '+':
					return a + b;
				case '*':
					return a * b;
				case '/':
					return a / b;
				case '%':
					return a % b;
				case '&':
					return a & b;
				case '|':
					return a | b;
				case parse::TokenType_kLsht:
					return a << b;
				case parse::TokenType_kRsht:
					return a >> b;
				case parse::TokenType_kEq:
					return a == b ? 1 : 0;
				case parse::TokenType_kNeq:
					return a == b ? 0 : 1;
				case parse::TokenType_kGeq:
					return a >= b ? 1 : 0;
				case '>':
					return a > b ? 1 : 0;
				case '<':
					return a < b ? 1 : 0;
				case parse::TokenType_kLeq:
					return a <= b ? 1 : 0;
				case parse::TokenType_kAndAnd:
					return a && b ? 1 : 0;
				case parse::TokenType_kOrOr:
					return a || b ? 1 : 0;
				}
				throw vm::Exception("invalid operator {}", op);
				return T();
			}
			template <> vm::Variant handle_binary_op(const float& a, const float& b, int op)
			{
				switch (op)
				{
				case '-':
					return a - b;
				case '+':
					return a + b;
				case '*':
					return a * b;
				case '/':
					return a / b;
				case parse::TokenType_kEq:
					return a == b ? 1 : 0;
				case parse::TokenType_kNeq:
					return a == b ? 0 : 1;
				case parse::TokenType_kGeq:
					return a >= b ? 1 : 0;
				case parse::TokenType_kLeq:
					return a <= b ? 1 : 0;
				case '>':
					return a > b ? 1 : 0;
				case '<':
					return a < b ? 1 : 0;
				case '%':
					return fmod(a, b);
				}
				throw vm::Exception("invalid operator {}", op);
				return 0.f;
			}

			template <> vm::Variant handle_binary_op(const vm::Vector& a, const vm::Vector& b, int op)
			{
				vm::Vector ret;
				ret.x = ret.y = ret.z = 0.f;
				switch (op)
				{
				case '-':
					ret.x = a.x - b.x;
					ret.y = a.y - b.y;
					ret.z = a.z - b.z;
					break;
				case '+':
					ret.x = a.x + b.x;
					ret.y = a.y + b.y;
					ret.z = a.z + b.z;
					break;
				case '*':
					ret.x = a.x * b.x;
					ret.y = a.y * b.y;
					ret.z = a.z * b.z;
					break;
				case '/':
					ret.x = a.x / b.x;
					ret.y = a.y / b.y;
					ret.z = a.z / b.z;
					break;
				default:
					throw vm::Exception("invalid operator {}", op);
					break;
				}
				return ret;
			}

			template <> vm::Variant handle_binary_op(const std::string& a, const std::string& b, int op)
			{
				switch (op)
				{
				case '+':
					return a + b;
				case parse::TokenType_kEq:
					return a == b ? 1 : 0;
				case parse::TokenType_kNeq:
					return a == b ? 0 : 1;
				}
				throw vm::Exception("invalid operator {}", op);
				return "";
			}

			vm::Variant binop(const vm::Variant& a, const vm::Variant& b, int op)
			{
				vm::Type a_index = (vm::Type)a.index();
				vm::Type b_index = (vm::Type)b.index();
				vm::Variant result;
				if (a_index == vm::Type::kString || b_index == vm::Type::kString)
				{
					if (a_index == vm::Type::kString)
						result = handle_binary_op(std::get<vm::String>(a), variant_to_string(b), op);
					else
						result = handle_binary_op(variant_to_string(a), std::get<vm::String>(b), op);
				}
				else
				{
					if (a_index == vm::Type::kVector && b_index == vm::Type::kVector)
					{
						result = handle_binary_op(std::get<vm::Vector>(a), std::get<vm::Vector>(b), op);
					}
					else if (a_index == vm::Type::kFloat && b_index == vm::Type::kFloat)
					{
						result = handle_binary_op(variant_to_number(a), variant_to_number(b), op);
					}
					else if (a_index == vm::Type::kFloat && b_index == vm::Type::kInteger)
					{
						result = handle_binary_op(variant_to_number(a), variant_to_number(b), op);
					}
					else if (a_index == vm::Type::kInteger && b_index == vm::Type::kFloat)
					{
						result = handle_binary_op(variant_to_number(a), variant_to_number(b), op);
					}
					else
					{
						result = handle_binary_op(variant_to_integer(a), variant_to_integer(b), op);
					}
				}
				return result;
			}
		};
	};
};