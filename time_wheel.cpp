#include "time_wheel.h"
#include <assert.h>
#include <cassert>
#include <string.h>
#include <stdio.h>

#if !defined(_WIN32)
// as windows GetTickCount() function.
inline auto GetTickCount() {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#else
// for GetTickCount() function.
#include <windows.h.>
#pragma warning(disable:4996)
#endif

namespace STimeWheelSpace {
	// system time, unit: ms
	static auto get_system_time() {
		return GetTickCount();
	}

	// safe copy string
	static inline void safeCopy(char *des, int des_len, const char *src) {
		if (!des || des_len <= 0) return;
		if (!src) {
			des[0] = '\0';
			return;
		}

		int len = int(strlen(src));
		if (len < des_len) {
			strcpy(des, src);
		} else {
			strncpy(des, src, des_len - 1);
			des[des_len - 1] = '\0';
		}
	}

	/// all kinds of macros start.
	// timer para check.
#define check_timer_para(delay, repeat)                   \
	if ((delay) < 0) {                                    \
		assert(false && "delay time error");              \
		return nullptr;                                   \
	}                                                     \
	assert(!((delay) == 0 && (repeat) > 0) && "circle timer");\
	if (delay == 0 && repeat > 0) {                       \
		return nullptr;                                   \
	}

	// increase_index macros.
#define increase_index(index, step, max_size) {           \
        (index) += (step);                                \
        (index) %= (max_size);                            \
    }

	// timer id check.
#define exist_timer_id_check(id, remove)                  \
    add_timer_ret ret = ADD_TIMER_FAIL;                   \
	if (id == invalid_timer_id) {                         \
		id = next_id();                                   \
	}                                                     \
	ret = _repeat_timer_check(remove, id);                \
	if (EXIST_NOT_REMOVE_RET == ret) {                    \
		return ret;                                       \
	}                                                     \

	// do add timer macro.for all add timer function.
#define do_add_timer(func, id, delay, timerType, data, remove, release_func) { \
      if (delay < 0) return ADD_TIMER_FAIL;                                 \
                                                                            \
      exist_timer_id_check((id), (remove));                                 \
	  wheel_info *info = CTimeWheel::instance().set_timer(func, data, id, delay, timerType, this); \
	  if (!info) return ADD_TIMER_FAIL;                                     \
      if (info == &wheel_info_) return ADD_TIMER_SUCC;                      \
      info->release = (release_func);                                       \
	  m_timer[id]   = info;                                                 \
      return ret;                                                           \
    }
	/// all kinds of macros end.

	CTimeWheel::CTimeWheel() : m_array(nullptr), m_index(0), m_next_objId(0) {
		assert(Max_array_size > 0 && "array size error");
		m_array = new list_head[Max_array_size];
		assert(m_array && "new memory error");

		// last time.
		m_last_time = get_system_time();

		// construct head. 
		for (uint32 i = 0; i < Max_array_size; i++) {
			INIT_LIST_HEAD(&m_array[i]);
		}

		bool r = m_pool.init(pool_start_size, pool_grow_size);
		assert(r && "init error");
	}

	CTimeWheel::~CTimeWheel() {
		if (!m_array) return;

		// clear.
		list_head *pos, *n;
		wheel_info *pinfo;
		for (uint32 i = 0; i < Max_array_size; i++) {
			list_head *list = &m_array[m_index];
			list_for_each_safe(pos, n, list) {
				if ((pinfo = list_entry(pos, wheel_info, link))) {
					this->_do_release(pinfo);
				} else {
					assert(false && "get value pointer error");
				}
				// delete it.
				list_del_init(pos);
			}
		}

		// delete array.
		delete[] m_array;
		m_array = nullptr;
	}

	CTimeWheel &CTimeWheel::instance() {
		static CTimeWheel instance;
		return instance;
	}

	void CTimeWheel::_release(wheel_info *pinfo) {
		if (!pinfo) return;
		m_pool.release_obj(pinfo);
	}

	void CTimeWheel::_do_release(wheel_info *pinfo) {
		if (!pinfo) return;

		// try to release user allocating data.
		if (pinfo->data.pvalue && pinfo->release) {
			(pinfo->release)(pinfo->data.pvalue);
			pinfo->data.pvalue = nullptr;
		}

		// try to remove it from register
		if (pinfo->reg && pinfo->state != timer_state_released) {
			pinfo->reg->remove_timer(pinfo);
		}

		// release
		this->_release(pinfo);
	}

	void CTimeWheel::update(uint32 delta) {
		if (delta <= 0) return;

		list_head *pos, *n;
		wheel_info *pinfo;
		while (delta-- > 0) {
			list_head *list = &m_array[m_index];
			list_for_each_safe(pos, n, list) {
				pinfo = list_entry(pos, wheel_info, link);
				assert(pinfo != nullptr && "pinfo is null");

				// killed or released outside.
				if (removable(pinfo)) {
					this->_do_release(pinfo);
					list_del_init(pos);
					continue;
				}

				// only it is running state can be done.
				if (pinfo->state == timer_state_running ||
					pinfo->state == timer_state_interrupted) {
					// can execute now.
					if (--pinfo->turn < 0) {
						if (pinfo->state == timer_state_running) {
							pinfo->func(&pinfo->data);
						}

						// remove first.
						list_del_init(pos);

						// can addable now?
						if (addable(pinfo)) {
							this->_add(pinfo);
						} else {
							this->_do_release(pinfo);
						}
					}
				}
			}

			// increase to next index.
			increase_index(m_index, 1, Max_array_size);
		}
	}

	void CTimeWheel::run() {
		auto now = get_system_time();
		this->update(uint32(now - m_last_time));
		m_last_time = now;
	}

	void CTimeWheel::_add(wheel_info *pinfo) {
		if (!pinfo) return;

		// next delay time.
		int32 delay = int32(pinfo->delay);

		// turns
		pinfo->turn = delay / Max_array_size;

		// index
		pinfo->index = delay % Max_array_size;

		// index check.
		increase_index(pinfo->index, m_index, Max_array_size);
		assert(pinfo->index >= 0 && pinfo->index < Max_array_size && "add_index error");

		// add tail.
		list_add_tail(&pinfo->link, &m_array[pinfo->index]);
	}

	wheel_info* CTimeWheel::_init_wheel_info(const timer_func& func, uint64 id,
		int32 delay, eTimerType timerType, Register *reg
	) {
		wheel_info *pInfo = m_pool.fetch_obj();
		assert(pInfo && "alloc wheel_info error");
		if (!pInfo) return nullptr;

		pInfo->id = id;
		pInfo->func = std::move(func);
		pInfo->delay = delay;
		pInfo->timerType = timerType;
		pInfo->state = timer_state_running;
		pInfo->reg = reg;
		pInfo->release = nullptr;
		pInfo->start_time = get_system_time();
		pInfo->objId = m_next_objId++;
		return pInfo;
	}

	uint32 CTimeWheel::get_all_timer() const {
		uint32 count = 0;
		for (uint32 i = 0; i < Max_array_size; i++) {
			list_head *pos, *n;
			list_head *list = &m_array[i];
			list_for_each_safe(pos, n, list) {
				count++;
			}
		}
		return count;
	}

	bool CTimeWheel::add_once_timer(const timer_func& func, int32 delay, const attach& data) {
		return nullptr != set_timer(func, data, 0, delay, onceType);
	}

	bool CTimeWheel::add_repeated_timer(const timer_func& func, int32 delay, const attach& data) {
		return nullptr != set_timer(func, data, 0, delay, repeatedType);
	}

	bool CTimeWheel::add_timer_at(const timer_func &func, int64 timestamp,
		const attach& data /*= attach_ */
	) {
		auto now = get_system_time();
		int32 delay = int32(timestamp - now);
		return add_once_timer(func, delay, data);
	}

	wheel_info* CTimeWheel::set_timer(const timer_func& func, void* data,
		uint64 id, int32 delay, eTimerType timerType, Register *reg
	) {
		attach a;a.pvalue = data;
		return set_timer(func, a, id, delay, timerType, reg);
	}

	wheel_info* CTimeWheel::set_timer(const timer_func& func, int64 data,
		uint64 id, int32 delay, eTimerType timerType, Register *reg
	) {
		attach a;a.ivalue = data;
		return set_timer(func, a, id, delay, timerType, reg);
	}

	wheel_info* CTimeWheel::set_timer(const timer_func& func, const char* data,
		uint64 id, int32 delay, eTimerType timerType, Register *reg
	) {
		attach a;
		safeCopy(a.svalue, attach_string_size, data);
		return set_timer(func, a, id, delay, timerType, reg);
	}

	wheel_info* CTimeWheel::set_timer(const timer_func& func, uint64 data,
		uint64 id, int32 delay, eTimerType timerType, Register *reg
	) {
		attach a;a.uvalue = data;
		return set_timer(func, a, id, delay, timerType, reg);
	}

	wheel_info* CTimeWheel::set_timer(const timer_func& func, decimal data,
		uint64 id, int32 delay, eTimerType timerType, Register *reg
	) {
		attach a;a.fvalue = data;
		return set_timer(func, a, id, delay, timerType, reg);
	}

	wheel_info* CTimeWheel::set_timer(const timer_func& func, const attach& data,
		uint64 id, int32 delay, eTimerType timerType, Register *reg
	) {
		check_timer_para(delay, timerType);
		if (delay <= 0) { // if delay <= 0, then just call it right now.
			attach a = data;
			// execute it right now.
			func(&a);
			// return special object.
			return &wheel_info_;
		}

		wheel_info *pinfo = _init_wheel_info(func, id, delay, timerType, reg);
		if (&data != &attach_) { // init attach exclude default attach_
			pinfo->data = data;
		} else {
			pinfo->data.pvalue = nullptr;
		}
		this->_add(pinfo);
		return pinfo;
	}

	//
	// timer register.
	//
	CTimerRegister::CTimerRegister() {
		m_timer.clear();
	}

	CTimerRegister::~CTimerRegister() {
		this->_release_all_timer();
	}

	add_timer_ret CTimerRegister::add_repeated_timer(const timer_func&& func,
		uint64 id, int32 delay, const attach &data, bool remove,
		void(*release_func)(void*)
	) {
		do_add_timer(func, id, delay, repeatedType, data, remove, release_func);
	}

	add_timer_ret CTimerRegister::add_once_timer(const timer_func&& func,
		uint64 id, int32 delay, const attach &data, bool remove,
		void(*release_func)(void*)
	) {
		do_add_timer(func, id, delay, onceType, data, remove, release_func);
	}

	add_timer_ret CTimerRegister::add_timer_at(const timer_func&& func,
		uint64 id, int64 timestamp, const attach &data, bool remove,
		void(*release_func)(void*)
	) {
		auto now = get_system_time();
		int32 delay = int32(timestamp - now);
		if (delay >= 0) {
			do_add_timer(func, id, delay, onceType, data, remove, release_func);
		} else {
			return ADD_TIMER_FAIL;
		}
	}

	add_timer_ret CTimerRegister::_repeat_timer_check(bool replace, uint64 id) {
		auto it = m_timer.find(id);
		if (it != m_timer.end()) {
			if (it->second->state != timer_state_killed) { // not killed.
				if (replace) {// new state, other state will be down.
					it->second->state = timer_state_replaced;
					return EXIST_REMOVE_RET;
				} else {
					return EXIST_NOT_REMOVE_RET;
				}
			}
		}
		return ADD_TIMER_SUCC;
	}

	uint64 CTimerRegister::next_id() const {
		uint64 maxId = 0;
		auto it = m_timer.begin();
		for (; it != m_timer.end(); it++) {
			if (it->first > maxId) {
				maxId = it->first;
			}
		}
		return maxId + 1;
	}

	void CTimerRegister::_out_put_timer(const wheel_info *timer) {
		printf("objId:%d,id:%lld, left time:%d",
			timer->objId, timer->id,
			int(get_left_time(timer->id))
		);
	}

	void CTimerRegister::traverse() {
		for (auto &e : m_timer) {
			_out_put_timer(e.second);
		}
	}

	add_timer_ret CTimerRegister::add_timer(const timer_func&& func,
		void *pData, uint64 id, int32 expire, bool loop, bool remove,
		void(*release_func)(void*)
	) {
		do_add_timer(func, id, expire, loop ? repeatedType : onceType, pData, remove, release_func);
	}

	add_timer_ret CTimerRegister::add_timer(const timer_func&& func,
		int64 ivalue, uint64 id, int32 expire, bool loop, bool remove,
		void(*release_func)(void*)
	) {
		do_add_timer(func, id, expire, loop ? repeatedType : onceType, ivalue, remove, release_func);
	}

	add_timer_ret CTimerRegister::add_timer(const timer_func&& func,
		const char svalue[], uint64 id, int32 expire, bool loop,
		bool remove, void(*release_func)(void*)
	) {
		do_add_timer(func, id, expire, loop ? repeatedType : onceType, svalue, remove, release_func);
	}

	add_timer_ret CTimerRegister::add_timer(const timer_func&& func,
		uint64 data, uint64 id, int32 expire, bool loop, bool remove,
		void(*release_func)(void*)
	) {
		do_add_timer(func, id, expire, loop ? repeatedType : onceType, data, remove, release_func);
	}

	add_timer_ret CTimerRegister::add_timer(const timer_func&& func,
		decimal data, uint64 id, int32 expire, bool loop,
		bool remove, void(*release_func)(void*)
	) {
		do_add_timer(func, id, expire, loop ? repeatedType : onceType, data, remove, release_func);
	}

	bool CTimerRegister::kill_timer(uint64 id) {
		if (this->_set_state(id, timer_state_killed)) {
			this->m_timer.erase(id);
			return true;
		} else {
			return false;
		}
	}

	void CTimerRegister::kill_all_timer() {
		for (auto &e : m_timer) {
			e.second->state = timer_state_killed;
			e.second->reg = nullptr;
		}
		m_timer.clear();
	}

	void CTimerRegister::_release_all_timer() {
		for (auto &e : m_timer) {
			e.second->state = timer_state_released;
			if (e.second->reg != nullptr) {
				assert(e.second->reg == this && "reg is not the same");
			}
			e.second->reg = nullptr;
		}
		m_timer.clear();
	}

	int64 CTimerRegister::get_left_time(uint64 id) const {
		auto it = m_timer.find(id);
		if (it == m_timer.end()) {
			return -1;
		}

		const wheel_info *pinfo = it->second;
		if (!pinfo) {
			return -1;
		}

		if (pinfo->state != timer_state_running) {
			return -1;
		}

		uint32 index = CTimeWheel::instance().get_index();
		int64 left_time = 0;
		if (index <= pinfo->index) {
			left_time = pinfo->index - index;
		} else {
			left_time = Max_array_size - (index - pinfo->index);
		}

		int64 turns = int64(pinfo->turn - 1);
		if (turns >= 0) {
			left_time += turns * int64(Max_array_size);
		}
		return left_time;
	}

	wheel_info* CTimerRegister::find_timer(uint64 id) {
		auto it = m_timer.find(id);
		if (it != m_timer.end()) {
			return it->second;
		} else {
			return nullptr;
		}
	}

	const wheel_info* CTimerRegister::find_timer(uint64 id) const {
		auto it = m_timer.find(id);
		if (it != m_timer.end()) {
			return it->second;
		} else {
			return nullptr;
		}
	}

	// must have id timer and proper state.
	bool CTimerRegister::has_timer(uint64 id) const {
		const wheel_info *wheel = this->find_timer(id);
		return wheel && wheel->state == timer_state_running;
	}

	bool CTimerRegister::_set_state(uint64 id, timer_state state) {
		auto it = m_timer.find(id);
		if (it != m_timer.end()) {
			it->second->state = state;
			return true;
		} else {
			return false;
		}
	}

	void CTimerRegister::remove_timer(wheel_info *info) {
		if (info == nullptr) return;

		auto it = m_timer.find(info->id);
		if (it != m_timer.end()) {
			if (it->second->objId == info->objId) {
				m_timer.erase(info->id);
			}
		}
	}

	uint32 CTimerRegister::get_timer_count() const {
		uint32 count = 0;
		for (const auto &e : m_timer) {
			if (this->has_timer(e.first)) {
				count++;
			}
		}
		return count;
	}

	const attach* CTimerRegister::get_timer_attach(uint64 id) {
		auto timerInfo = this->find_timer(id);
		if (!timerInfo) {
			return nullptr;
		} else {
			return &timerInfo->data;
		}
	}

	bool CTimerRegister::interrupt(uint64 id) {
		return this->_set_state(id, timer_state_interrupted);
	}

	bool CTimerRegister::reStart(uint64 id) {
		return this->_set_state(id, timer_state_running);
	}

}