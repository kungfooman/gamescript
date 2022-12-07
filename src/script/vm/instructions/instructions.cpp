#include "instructions.h"
#include <script/vm/virtual_machine.h>
#include <core/time.h>

namespace script
{
	namespace vm
	{

		static int get_vector_index(const std::string &s)
		{
			if (s[0] == 'x')
				return 0;
			if (s[0] == 'y')
				return 1;
			if (s[0] == 'z')
				return 2;
			if (s[0] == '0')
				return 0;
			if (s[0] == '1')
				return 1;
			if (s[0] == '2')
				return 2;
			throw vm::Exception("vector out of bounds");
		}

		std::string PushInteger::to_string()
		{
			return common::format("PushInteger {}", value);
		}
		void CallFunctionPointer::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			vm::ObjectPtr obj = thread_context->function_context().self_object;
			if (is_method_call)
			{
				obj = thread_context->context()->get_object(0);
				thread_context->pop();
			}
			auto vfp = thread_context->context()->get_variant(0);
			thread_context->pop();
			if (vfp.index() != (int)vm::Type::kFunctionPointer)
				throw vm::Exception("{} is not a function pointer", vfp.index());
			auto fp = std::get<vm::FunctionPointer>(vfp);
			std::string ref = fp.file;
			std::replace(ref.begin(), ref.end(), '\\', '/');
			if (is_threaded)
			{
				thread_context->push(vm.exec_thread(thread_context, obj, ref, fp.name, numargs, is_method_call));
			}
			else
				vm.call_function(thread_context, obj, ref, fp.name, this->numargs, is_method_call);
		}
		void CallFunctionFile::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			vm::ObjectPtr obj = thread_context->function_context().self_object;
			if (is_method_call)
			{
				obj = thread_context->context()->get_object(0);
				thread_context->pop();
			}
			std::string ref = file;
			std::replace(ref.begin(), ref.end(), '\\', '/');
			if (is_threaded)
			{
				thread_context->push(vm.exec_thread(thread_context, obj, ref, this->function, numargs, is_method_call));
			}
			else
				vm.call_function(thread_context, obj, ref, this->function, this->numargs, is_method_call);
		}
		void CallFunction::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			vm::ObjectPtr obj = thread_context->function_context().self_object;
			if (is_method_call)
			{
				obj = thread_context->context()->get_object(0);
				thread_context->pop();
			}
			if (is_threaded)
			{
				thread_context->push(vm.exec_thread(thread_context, obj, thread_context->current_file(), this->function,
													numargs, is_method_call));
			}
			else
				vm.call_function(thread_context, obj, thread_context->current_file(), this->function, this->numargs,
								 is_method_call);
		}
		void WaitTill::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			vm::ObjectPtr obj;
			if (is_method_call)
			{
				obj = thread_context->context()->get_object(0);
				thread_context->pop();
			}
			if (!obj)
				throw vm::Exception("no obj");
			std::string evstr = thread_context->context()->get_string(0);
			thread_context->pop();
			std::vector<std::string> vars;
			for (size_t i = 0; i < this->numargs; ++i)
			{
				vars.push_back(thread_context->context()->get_string(0));
				thread_context->pop();
			}
			std::reverse(vars.begin(), vars.end());
			vm.waittill(thread_context, obj, evstr, vars);
		}
		void JumpNotZero::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			if ((vm.get_flags() & vm::flags::kZF) != vm::flags::kZF)
			{
				if (!this->dest.expired())
				{
					size_t idx = this->dest.lock()->label_index;
					thread_context->jump(idx);
				}
			}
		}
		void Constant1::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			thread_context->push(1);
		}
		void Constant0::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			thread_context->push(0);
		}
		void JumpZero::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			if (vm.get_flags() & vm::flags::kZF)
			{
				if (!this->dest.expired())
				{
					size_t idx = this->dest.lock()->label_index;
					thread_context->jump(idx);
				}
			}
		}
		void Jump::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			if (!this->dest.expired())
			{
				size_t idx = this->dest.lock()->label_index;
				thread_context->jump(idx);
			}
		}
		void Test::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			auto v = thread_context->context()->get_variant(0);
			thread_context->pop();
			if (v.index() == (int)vm::Type::kInteger)
			{
				int intval = std::get<vm::Integer>(v);
				if (intval == 0)
					vm.set_flags(vm.get_flags() | vm::flags::kZF);
				else
					vm.set_flags(vm.get_flags() & ~vm::flags::kZF);
			}
			else if (v.index() == (int)vm::Type::kUndefined)
			{
				vm.set_flags(vm.get_flags() | vm::flags::kZF);
			}
			else
				throw vm::Exception("unexpected {}", v.index());
		}
		void Label::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
		}
		void BinOp::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			auto a = thread_context->context()->get_variant(0);
			auto b = thread_context->context()->get_variant(1);
			thread_context->pop();
			thread_context->pop();
			thread_context->push(vm.binop(a, b, op));
		}
		void WaitTillFrameEnd::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			struct ThreadLockWaitFrame : vm::ThreadLock
			{
				VirtualMachine& vm;
				size_t frame;
				ThreadLockWaitFrame(VirtualMachine& vm_, size_t frame_) : frame(frame_), vm(vm_)
				{
				}
				virtual void notify(NotifyEvent&)
				{
				}
				virtual bool locked()
				{
					return vm.get_frame_number() == frame;
				}
			};
			auto l = std::make_unique<ThreadLockWaitFrame>(vm, vm.get_frame_number());
			thread_context->m_locks.push_back(std::move(l));
		}
		void Wait::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			float duration = thread_context->context()->get_float(0);
			thread_context->pop();
			struct ThreadLockWaitDuration : vm::ThreadLock
			{
				uint32_t end_time = 0;
				virtual void notify(NotifyEvent&)
				{
				}
				virtual bool locked()
				{
					return core::time_milliseconds() < end_time;
				}
			};
			auto l = std::make_unique<ThreadLockWaitDuration>();
			l->end_time = core::time_milliseconds() + duration * 1000.f;
			thread_context->m_locks.push_back(std::move(l));
		}
		void Ret::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			thread_context->ret();
		}
		void Not::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			auto i = thread_context->context()->get_int(0);
			thread_context->pop();
			thread_context->push(~i);
		}
		void LogicalNot::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			Variant v = thread_context->context()->get_variant(0);
			thread_context->pop();
			if (v.index() == (int)vm::Type::kInteger)
			{
				thread_context->push(!std::get<vm::Integer>(v));
			}
			else if (v.index() == (int)vm::Type::kUndefined)
			{
				thread_context->push(1);
			}
			else
				throw vm::Exception("unexpected {}", v.index());
		}
		void LoadObjectFieldValue::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			//TODO: FIXME we can't actually load anything if ref is undefined...
			auto ref = thread_context->pop();
			auto prop = thread_context->context()->get_string(0);
			thread_context->pop(1);

			if (ref.index() == (int)vm::Type::kVector)
			{
				auto vec = std::get<vm::Vector>(ref);
				int propidx = get_vector_index(prop);
				thread_context->push(vec[propidx]);
				return;
			}

			//TODO: FIXME something is still undefined and won't load properly...
			//something with the spawnlogic, maybe it's just not getting any spawns from entities and just stays undefined idk
			#if 1
			if (ref.index() == (int)vm::Type::kUndefined)
			{
				ref = std::make_shared<Object>("object created from undefined");
			}
			#endif
			if (ref.index() != (int)vm::Type::kObject)
			{
				throw vm::Exception("expected object got {}", ref.index());
			}
			std::shared_ptr<vm::Object> obj = std::get<vm::ObjectPtr>(ref);
			if (prop == "size")
			{
				thread_context->push(vm::Integer(obj->fields.size()));
			}
			else
			{
				try
				{
					auto fv = obj->get_field(util::string::to_lower(prop));
					if (fv)
						thread_context->push(*fv);
					else
						thread_context->push(vm::Undefined());
				}
				catch (...)
				{
					throw vm::Exception("failed getting field {}", prop);
				}
			}
		}
		void LoadObjectFieldRef::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			auto ref = thread_context->pop_ref();
			auto prop = thread_context->context()->get_string(0);
			thread_context->pop(1);
			if (prop == "size")
				throw vm::Exception("size is read-only");

			struct Visit
			{
				ThreadContext* thread_context;
				VirtualMachine& vm;
				std::string prop;

				Visit(VirtualMachine& vm_, ThreadContext* tc, const std::string& prop_)
					: vm(vm_), thread_context(tc), prop(prop_)
				{
				}
				void operator()(Identifier& v)
				{
					auto* vr = vm.get_variable_reference(thread_context, util::string::to_lower(v.name));

					vm::Reference r;
					if (std::holds_alternative<Vector>(*vr))
					{
						r.object = std::get_if<Vector>(vr);
						r.index = get_vector_index(prop);
					}
					else if (std::holds_alternative<ObjectPtr>(*vr))
					{
						r.object = std::get<ObjectPtr>(*vr);
						r.field = prop;
					}
					else if (std::holds_alternative<Undefined>(*vr))
					{
						auto obj = std::make_shared<Object>("object created from undefined");
						r.object = obj;
						r.field = prop;
					}
					else
					{
						throw vm::Exception("not a lvalue");
					}
					thread_context->push(r);
				}
				void operator()(Vector*& v)
				{
					throw vm::Exception("nested vector [][] not supported, because vector[index][index2] doesn't return a lvalue");
				}
				void operator()(ObjectPtr& v)
				{
					vm::Reference r;
					auto *fv = v->get_field(util::string::to_lower(prop));
					if (std::holds_alternative<Vector>(*fv))
					{
						r.object = std::get_if<Vector>(fv);
						r.index = get_vector_index(prop);
					}
					else if (std::holds_alternative<ObjectPtr>(*fv))
					{
						r.object = std::get<ObjectPtr>(*fv);
						r.field = prop;
					}
					else
					{
						throw vm::Exception("not a lvalue");
					}
					thread_context->push(r);
				}
			};

			std::visit(Visit(vm, thread_context, util::string::to_lower(prop)), ref.object);
		}
		void StoreRef::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			auto ref = thread_context->pop_ref();
			vm::Variant value = thread_context->pop(1);

			struct Visit
			{
				ThreadContext* thread_context;
				VirtualMachine& vm;
				vm::Variant& value;
				vm::Reference& ref;

				Visit(VirtualMachine& vm_, ThreadContext* tc, vm::Variant& value_, vm::Reference& ref_)
					: vm(vm_), thread_context(tc), value(value_), ref(ref_)
				{
				}
				void operator()(Identifier& v)
				{
					auto* vr = vm.get_variable_reference(thread_context, util::string::to_lower(v.name));
					*vr = value;
				}
				void operator()(Vector*& v)
				{
					(*v)[ref.index] = vm.variant_to_number(value);
				}
				void operator()(ObjectPtr& v)
				{
					try
					{
						v->set_field(util::string::to_lower(ref.field), value);
					}
					catch (...)
					{
						throw vm::Exception("failed setting field {} to {} on object", ref.field,
											vm.variant_to_string(value));
					}
				}
			};

			std::visit(Visit(vm, thread_context, value, ref), ref.object);
		}
		void LoadValue::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			thread_context->push(vm.get_variable(thread_context, util::string::to_lower(variable_name)));
		}
		void LoadRef::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			thread_context->push(
				vm::Reference{.object = vm::Identifier{.name = util::string::to_lower(variable_name)}});
		}
		void Nop::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			throw vm::Exception("unhandled instruction {}", __LINE__);
		}
		void PushUndefined::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			thread_context->push(vm::Undefined());
		}
		void PushAnimationString::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			vm::Animation a;
			a.reference = value;
			thread_context->push(a);
		}
		void PushFunctionPointer::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			vm::FunctionPointer fp;
			fp.file = this->file;
			fp.name = this->function;
			thread_context->push(fp);
		}
		void PushLocalizedString::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			vm::LocalizedString s;
			s.reference = this->value;
			thread_context->push(s);
		}
		void PushString::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			vm::String s = value;
			thread_context->push(s);
		}
		void PushArray::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			//TODO: FIXME don't use object as array
			vm::ObjectPtr o = std::make_shared<vm::Object>("pusharray");
			thread_context->push(o);
		}
		void PushVector::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			vm::Vector v;
			v.x = thread_context->context()->get_float(0);
			v.y = thread_context->context()->get_float(1);
			v.z = thread_context->context()->get_float(2);
			thread_context->pop(3);
			thread_context->push(v);
		}
		void PushNumber::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			vm::Number v = value;
			thread_context->push(v);
		}
		void PushInteger::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			vm::Integer v = value;
			thread_context->push(v);
		}
		void Pop::execute(VirtualMachine& vm, ThreadContext *thread_context)
		{
			thread_context->pop();
		}
	}; // namespace vm
};	   // namespace script
