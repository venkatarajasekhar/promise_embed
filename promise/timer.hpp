#pragma once
#ifndef INC_TIMER_HPP_
#define INC_TIMER_HPP_

//#include "promise.hpp"

#define TT_TICKS_PER_SECOND 1000

namespace promise{

struct timer_global {
    pm_list  timers_;
    volatile uint64_t current_ticks_;
    volatile uint64_t time_offset_;             /* tt_set_time() only set this value */

/* Why TT_TICKS_DEVIDER use (4096*1024),
   to use this value, maximum of TT_TICKS_PER_SECOND_1 is (4096*1024),
   1000*TT_TICKS_PER_SECOND_1 < pow(2,32), and will not overflow in tt_ticks_to_msec
 */
#define TT_TICKS_DEVIDER    (4096*1024)
    volatile uint64_t TT_TICKS_PER_SECOND_1;    /*  TT_TICKS_DEVIDER/TT_TICKS_PER_SECOND for fast devision */
};

struct pm_timer {

    static inline timer_global *get_global(){
        static timer_global *global = nullptr;
        if(global == nullptr)
            global = pm_stack_new<timer_global>();
        return global;
    }

    static void init_system(uint32_t systick_frequency){
        timer_global *global = pm_timer::get_global();
        global->TT_TICKS_PER_SECOND_1 = TT_TICKS_DEVIDER / TT_TICKS_PER_SECOND;
        SysTick_Config(systick_frequency / TT_TICKS_PER_SECOND);
    }
    
    static void increase_ticks(){
        timer_global *global = pm_timer::get_global();
        global->current_ticks_++;
    }

    static uint64_t get_time(){
        timer_global *global = pm_timer::get_global();
        uint64_t u64_ticks_offset = global->time_offset_ * TT_TICKS_PER_SECOND;
        uint64_t u64_sec = (global->current_ticks_ + u64_ticks_offset) / TT_TICKS_PER_SECOND;
        return u64_sec;
    }

    static uint64_t set_time(uint64_t new_time){
        timer_global *global = pm_timer::get_global();
        uint64_t rt = global->time_offset_;
        global->time_offset_ = new_time;
        return rt;
    }

    static uint32_t get_ticks(void){
        timer_global *global = pm_timer::get_global();
        return global->current_ticks_;
    }

    static uint32_t ticks_to_msec(uint32_t ticks){
        timer_global *global = pm_timer::get_global();
        uint64_t u64_msec = 1000 * (uint64_t)ticks * global->TT_TICKS_PER_SECOND_1 / TT_TICKS_DEVIDER;
        uint32_t msec = (u64_msec >  (uint64_t)~(uint32_t)0
            ? ~(uint32_t)0 : (uint32_t)u64_msec);
        return msec;
    }

    static uint32_t msec_to_ticks(uint32_t msec){
        //timer_global *global = pm_timer::get_global();
        //uint64_t u64_ticks = TT_TICKS_PER_SECOND * (uint64_t)msec / 1000;
        //Do not use devider for fast and small size
        uint64_t u64_ticks = TT_TICKS_PER_SECOND * (uint64_t)msec * (TT_TICKS_DEVIDER / 1000) / TT_TICKS_DEVIDER;
        
        uint32_t ticks = (u64_ticks > (uint64_t)(uint32_t)0xFFFFFFFF
            ? (uint32_t)0xFFFFFFFF : (uint32_t)u64_ticks);

        return ticks;
    }

    static void run(){
        timer_global *global = pm_timer::get_global();

        uint32_t current_ticks = pm_timer::get_ticks ();

        pm_list *node = global->timers_.next();
        while(node != &global->timers_){
            pm_memory_pool_buf_header *header = pm_container_of(node, &pm_memory_pool_buf_header::list_);
            pm_timer *timer = reinterpret_cast<pm_timer *>(pm_memory_pool_buf_header::to_ptr(header));

            int32_t ticks_to_wakeup = (int32_t)(timer->wakeup_ticks_ - current_ticks);

            if(ticks_to_wakeup <= 0){
                pm_list *node_next = node->next();
                defer_list::attach(timer->defer_);
                node->detach();
                pm_delete(timer);
                node = node_next;
            }
            else break;
        }
    }
    
    static void kill__(Defer &defer){
#ifdef PM_DEBUG
        pm_assert(defer->type_ == PM_TYPE_TIMER);
#endif

        timer_global *global = pm_timer::get_global();

        pm_list *node = global->timers_.next();
        while(node != &global->timers_){
            pm_memory_pool_buf_header *header = pm_container_of(node, &pm_memory_pool_buf_header::list_);
            pm_timer *timer = reinterpret_cast<pm_timer *>(pm_memory_pool_buf_header::to_ptr(header));

            pm_list *node_next = node->next();
            if(timer->defer_ == defer){
                node->detach();
                pm_delete(timer);
            }
            node = node_next;
        }
    }

    static void kill(Defer &defer){
        if(defer.operator->()){
            Defer no_ref = defer;
            defer.clear();

            if(no_ref->status_ == Promise::kInit){
                pm_timer::kill__(no_ref);
                defer_list::remove(no_ref);
                no_ref.reject();
            }
        }
    }

    static void direct_run(Defer &defer){
        if(defer.operator->()){
            Defer no_ref = defer;
            defer.clear();

            if(no_ref->status_ == Promise::kInit){
                pm_timer::kill__(no_ref);
                defer_list::remove(no_ref);
                no_ref.resolve();
            }
        }
    }

    //pm_timer(const Defer &defer)
    //    : defer_(defer){
    //}
    pm_timer()
        : defer_(){
    }

    void start2(uint32_t ticks){
        timer_global *global = pm_timer::get_global();

        uint32_t current_ticks = pm_timer::get_ticks ();
        wakeup_ticks_ = current_ticks + ticks;

        pm_list *node = global->timers_.next();
        for(; node != &global->timers_; node = node->next()){
            pm_memory_pool_buf_header *header = pm_container_of(node, &pm_memory_pool_buf_header::list_);
            pm_timer *timer = reinterpret_cast<pm_timer *>(pm_memory_pool_buf_header::to_ptr(header));
            int32_t ticks_to_wakeup = (int32_t)(timer->wakeup_ticks_ - current_ticks);
            if (ticks_to_wakeup >= 0 && (uint32_t)ticks_to_wakeup > ticks)
                break;
        }

        pm_memory_pool_buf_header *header = pm_memory_pool_buf_header::from_ptr(this);
        pm_list *list = &header->list_;
        pm_assert(header->ref_count_ > 0);
        if(!list->empty())
            list->detach();
        node->attach(list);
    }

    void start(uint32_t msec){
        start2(msec_to_ticks(msec));
    }

//private:
    uint32_t wakeup_ticks_;
    Defer defer_;
};

inline Defer delay_ticks(uint32_t ticks) {
    pm_timer *timer = pm_new<pm_timer>();
    return newPromise([ticks, timer](const Defer &d){
#ifdef PM_DEBUG
        d->type_ = PM_TYPE_TIMER;
#endif
        timer->defer_ = d;
        timer->start2(ticks);
    });
}

inline Defer delay_ms(uint32_t msec) {
    return delay_ticks(pm_timer::msec_to_ticks(msec));
}

inline Defer delay_s(uint32_t sec) {
    uint64_t u64_ticks = TT_TICKS_PER_SECOND * (uint64_t)sec;
    uint32_t sleep_ticks = (u64_ticks > (uint64_t)(uint32_t)0xFFFFFFFF
        ? (uint32_t)0xFFFFFFFF : (uint32_t)u64_ticks);
    return delay_ticks(sleep_ticks);
}

inline Defer yield(){
    return delay_ticks(0);
}

inline void kill_timer(Defer &defer){
    return pm_timer::kill(defer);
}

inline void direct_run_timer(Defer &defer){
    return pm_timer::direct_run(defer);
}

/* Loop while func call resolved */
template <typename FUNC>
inline Defer delay_while(FUNC func) {
    return newPromise(func).then([]() {
        return yield();
    }).then([func]() {
        return delay_while(func);
    });
}

}
#endif

