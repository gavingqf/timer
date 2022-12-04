
// note  : new timer for 1ms unit.
// author: gavingqf@126.com, 2019/9/6
// idea  : limited array for unlimited time.
// using high efficient double list, now it can support 1500w timers at the same time.

// Copyright (c) 2019 - 2020 gavingqf (gavingqf@126.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <functional>
#include <map>
#include <utility>
#include <memory>
#include <tuple>
#include "list.h"
#include "../pool/objpool.h"

namespace STimeWheelSpace {
	// == typedef start, !!void* is attach* object.
	typedef std::function<void(void*)> timer_func;
	typedef unsigned int               uint32;
	typedef int                        int32;
	typedef unsigned short             uint16;
	typedef short                      int16;
	typedef unsigned char              uint8;
	typedef char                       int8;
	typedef double                     decimal;
	typedef struct list_head           list_head;

	// int64 and uint64
#ifndef __IINT64_DEFINED
#define __IINT64_DEFINED
#if defined(_MSC_VER) || defined(__BORLANDC__)
	typedef __int64 int64;
#else
	typedef long long int64;
#endif
#endif

#ifndef __IUINT64_DEFINED
#define __IUINT64_DEFINED
#if defined(_MSC_VER) || defined(__BORLANDC__)
	typedef unsigned __int64 uint64;
#else
	typedef unsigned long long uint64;
#endif
#endif
	// == typedef end
	typedef uint64 timerIdType;

	class CTimerRegister;

	// Add timer return value.
	typedef enum {
		ADD_TIMER_FAIL = -1,     // add fail, normally it is memory allocate fail or < 0 delay.
		ADD_TIMER_SUCC = 0,      // no repeated and success
		EXIST_REMOVE_RET = 1,    // repeated and remove it.
		EXIST_NOT_REMOVE_RET = 2,// repeated and not remove it.
	} add_timer_ret;

	// Timer type
	typedef enum eTimerType {
		onceType = 0,            // once type
		repeatedType = 1,        // repeated type
	} eTimerType;

	// digital value value.
	typedef union max_digital_value {
		uint64  udata;
		decimal fdata;
	} max_digital_value;

	// == const variable basic start
	// timer array size.
	static constexpr uint32 Max_array_size = (1 * 60 * 1000 - 1);

	// pool init size parameter.
	static constexpr uint32 pool_start_size = 32;
	static constexpr uint32 pool_grow_size = 8;

	// attach string size for attach.svalue.
	static constexpr uint32 attach_string_size = uint32(sizeof(max_digital_value) + 1);

	// invalid timer id definition: must use it carefully.
	static constexpr uint32 invalid_timer_id = uint32(~0);
	// == const variable basic end

		// timer state
	typedef enum {
		timer_state_invalid = 0,                 // invalid state
		timer_state_running,                     // run state
		timer_state_interrupted,                 // interrupt state
		timer_state_killed,                      // killed state
		// the follows state can not get outside.
		timer_state_replaced,                    // replace state.
		timer_state_released,                    // register released state.
	} timer_state;

	typedef CTimerRegister Register;

	// attach data.
	// integer(sign and unsign, also can for boolean), pointer, string and double attach.
	typedef union {
		void    *pvalue;                   // pointer data
		int64   ivalue;                    // int data.
		uint64  uvalue;                    // uint data.
		decimal fvalue;                    // double data
		char    svalue[attach_string_size];// string.
	} attach;

	// global attach variable, just for default attach.
	static attach attach_ = { nullptr };

	// timer para transfer macro.
#ifndef Null
#define Null ((void*)nullptr)
#endif
#ifndef to_int64
#define to_int64(num)   (int64(num))
#endif
#ifndef to_uint64
#define to_uint64(num)  (uint64(num))
#endif
#ifndef to_decimal
#define to_decimal(num) (decimal(num))
#endif
#ifndef to_pointer
#define to_pointer(p)   ((void*)(p))
#endif

	// Time wheel info
	typedef struct {
		uint64            id;          // timer id(it is not a unique id)
		int32             turn;        // must signed.
		uint32            delay;       // timer time: ms, if delay == 0, then execute timer right now.
		timer_func        func;        // callback lambda
		attach            data;        // attach.
		void(*release)(void*);         // data release func.
		eTimerType        timerType;   // as eTimerType.
		uint8             state;       // state, as timer_state
		Register          *reg;        // register pointer.
		uint32            index;       // index of array.
		list_head         link;        // list head to form a double queue.
		int64             start_time;  // timer start time.
		uint32            objId;       // timer object id, it is an unique object id.
	} wheel_info;
	inline bool addable(wheel_info *info) {   // must add to queue.
		return (repeatedType == info->timerType && timer_state_running == info->state)
			|| timer_state_interrupted == info->state;
	}
	inline bool removable(wheel_info *info) { // must remove from queue.
		return !(timer_state_running == info->state ||
			timer_state_interrupted == info->state);
	}
	static wheel_info wheel_info_;     // global wheel info.

	// time wheel timer class.
	class CTimeWheel final {
	protected:
		CTimeWheel();
		virtual ~CTimeWheel();
		// can not copyable class.
		const CTimeWheel& operator=(const CTimeWheel& rhs) = delete;
		CTimeWheel(const CTimeWheel& rhs) = delete;
		typedef SObjPoolSpace::object_pool<wheel_info, true> wheel_pool;

	public:
		static CTimeWheel &instance();

		// update: delta tick.
		void              update(uint32 delta);

		// just run independently.
		void              run();

		// current index.
		uint32            get_index() const { return m_index; }

		// all timer count
		uint32            get_all_timer() const;

		// once timer
		bool              add_once_timer(const timer_func& func,
			int32 delay, const attach& data = attach_
		);

		// repeat timer
		bool              add_repeated_timer(const timer_func& func,
			int32 delay, const attach& data = attach_
		);

		// add once timer at timestamp.
		bool              add_timer_at(const timer_func &func, 
			int64 timestamp, const attach& data = attach_
		);

	public:
		// all kinds of override set_timer.
		// attach data(real add timer function)
		wheel_info*       set_timer(const timer_func& func, const attach& data, uint64 id,
			int32 delay, eTimerType timerType, Register *reg = nullptr
		);

		// void* data.
		wheel_info*       set_timer(const timer_func& func, void* data, uint64 id,
			int32 delay, eTimerType timerType, Register *reg = nullptr
		);
		// int64 data.
		wheel_info*       set_timer(const timer_func& func, int64 data, uint64 id,
			int32 delay, eTimerType timerType, Register *reg = nullptr
		);
		// uint64 data.
		wheel_info*       set_timer(const timer_func& func, uint64 data, uint64 id,
			int32 delay, eTimerType timerType, Register *reg = nullptr
		);
		// const char* data.
		wheel_info*       set_timer(const timer_func& func, const char* data, uint64 id,
			int32 delay, eTimerType timerType, Register *reg = nullptr
		);
		// const char* data.
		wheel_info*       set_timer(const timer_func& func, decimal data, uint64 id,
			int32 delay, eTimerType timerType, Register *reg = nullptr
		);

	protected:
		// add timer info
		void              _add(wheel_info *pinfo);

		// remove timer info.
		void              _release(wheel_info *pinfo);
		void              _do_release(wheel_info *pinfo);

		// fetch info and init.
		wheel_info*       _init_wheel_info(const timer_func& func, uint64 id,
			int32 delay, eTimerType timerType, Register *reg
		);

	private:
		// list array.
		list_head *m_array;

		// wheel index
		uint32     m_index;

		// wheel info pool.
		wheel_pool m_pool;

		// last time: ms
		int64      m_last_time;

		// timer object id.
		uint32     m_next_objId;
	};


	// add right value reference.
	class CTimerRegister {
	public:
		CTimerRegister();
		virtual ~CTimerRegister();
		// can not copyable class.
		const CTimerRegister& operator =(const CTimerRegister& rhs) = delete;
		CTimerRegister(const CTimerRegister& rhs) = delete;
		typedef std::map<uint64, wheel_info*> timer_map;

	public:
		// repeated timer.
		add_timer_ret   add_repeated_timer(const timer_func&& func,
			uint64 id, int32 delay, const attach &data = attach_,
			bool remove = true, void(*release_func)(void*) = nullptr
		);

		// once timer.
		add_timer_ret   add_once_timer(const timer_func&& func,
			uint64 id, int32 delay, const attach &data = attach_,
			bool remove = true, void(*release_func)(void*) = nullptr
		);

		// run at timestamp
		add_timer_ret   add_timer_at(const timer_func&& func,
			uint64 id, int64 timestamp, const attach &data = attach_,
			bool remove = true, void(*release_func)(void*) = nullptr
		);

#if defined(_WIN32)
		// Variadic Templates function
		template <class Fn, class... Args>
		struct TimerPara {
			Fn func;
			std::tuple<Args...> funcPara;
			TimerPara(Fn&& fn, Args&&... args) : func(fn), funcPara(args...) {}
			TimerPara() = delete;
			~TimerPara() = default;
		};

		// once timer
		template <class Fn, class... Args>
		add_timer_ret  add_var_once_timer(uint32 id, int32 delay, Fn&& Fx, Args&&... Ax) {
			using ParaType = TimerPara<Fn, Args...>;
			auto *para = new ParaType(std::forward<Fn>(Fx), std::forward<Args>(Ax)...);
			// try to call once timer.
			return this->add_timer([=](void *p) {
				assert(p && "timer parameter is nil");
				auto *detailPara = (ParaType*)(((attach*)p)->pvalue);
				assert(detailPara && "para is nil");
				if (!detailPara) { return; }

				// call related function
				std::apply(detailPara->func, detailPara->funcPara);
				delete detailPara;
			}, (void*)para, id, delay, false, true, nullptr);
		}
		// repeated timer
		template <class Fn, class... Args>
		add_timer_ret  add_var_repeated_timer(uint32 id, int32 delay, Fn&& Fx, Args&&... Ax) {
			using ParaType = TimerPara<Fn, Args...>;
			auto *para = new ParaType(std::forward<Fn>(Fx), std::forward<Args>(Ax)...);
			// try to call repeated timer.
			return this->add_timer([=](void *p) {
				assert(p && "timer parameter is nil");
				auto *detailPara = (ParaType*)(((attach*)p)->pvalue);
				assert(detailPara && "para is nil");
				if (!detailPara) { return; }

				// call related function
				std::apply(detailPara->func, detailPara->funcPara);
			}, (void*)para, id, delay, true, true, nullptr);
		}
#endif

	public: // all following add_timer will be removed(deprecated).
		// add timer, return as add_timer_ret(the same as follows)
		// id can be INVALID_TIMER_ID to ignore timer id.
		// all add_timer function if it is lambda, can not use & to use parameter.
		// int64 para
		add_timer_ret    add_timer(const timer_func&& func,
			int64 data, uint64 id, int32 delay, bool loop = false,
			bool remove = true, void(*release_func)(void*) = nullptr
		);
		// uint64 para
		add_timer_ret    add_timer(const timer_func&& func,
			uint64 data, uint64 id, int32 delay, bool loop = false,
			bool remove = true, void(*release_func)(void*) = nullptr
		);
		// void* para
		add_timer_ret   add_timer(const timer_func&& func,
			void *data, uint64 id, int32 delay, bool loop = false,
			bool remove = true, void(*release_func)(void*) = nullptr
		);
		// const char[]
		add_timer_ret    add_timer(const timer_func&& func,
			const char data[], uint64 id, int32 delay, bool loop = false,
			bool remove = true, void(*release_func)(void*) = nullptr
		);
		add_timer_ret    add_timer(const timer_func&& func,
			decimal data, uint64 id, int32 delay, bool loop = false,
			bool remove = true, void(*release_func)(void*) = nullptr
		);

		// kill timer
		bool             kill_timer(uint64 id);

		// interrupt timer.
		bool             interrupt(uint64 id);

		// restart timer.
		bool             reStart(uint64 id);

		// kill all timer.
		void             kill_all_timer();

		// remove id timer. called only at CTimeWheel class
		// it must check whether it is the same timer.
		void             remove_timer(wheel_info *info);

		// find timer info
		wheel_info*      find_timer(uint64 id);
		const wheel_info*find_timer(uint64 id) const;

		// has timer(must be running state)
		bool             has_timer(uint64 id) const;

		// get left time. return -1 if has not id timer.
		int64            get_left_time(uint64 id) const;

		// next timer id.
		uint64           next_id() const;

		// iterator all timers
		void             traverse();

		// get timer count
		uint32           get_timer_count() const;

		// get timer attach.
		const attach*    get_timer_attach(uint64 id);

	protected:
		void             _out_put_timer(const wheel_info *timer);

		// set id timer state.
		bool             _set_state(uint64 id, timer_state state);

		// can not called outside: release all timer data, I am released normally.
		void             _release_all_timer();

		// timer check: remove denote whether remove existed timer.
		add_timer_ret    _repeat_timer_check(bool remove, uint64 id);

	protected:
		timer_map m_timer;
	};
}