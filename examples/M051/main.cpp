#include <stdio.h>
#include <string>
#include "M051Series.h"         // For M051 series
#include "promise/promise.hpp"


#define LED0_GPIO_GRP	P3
#define LED0_GPIO_BIT	4
#define LED1_GPIO_GRP	P3
#define LED1_GPIO_BIT	5
#define LED2_GPIO_GRP	P3
#define LED2_GPIO_BIT	6
#define LED3_GPIO_GRP	P3
#define LED3_GPIO_BIT	7

extern "C" {
/* For debug usage */
uint32_t g_alloc_size = 0;
uint32_t g_stack_size = 0;
uint32_t g_promise_call_len = 0;
}


using namespace promise;

#define output_func_name() do{ printf("in function %s, line %d\n", __func__, __LINE__); } while(0)

char test1() {
    output_func_name();
    return '3';
}

int test2() {
    //printf("c = %d\n", c);
    output_func_name();
    return 5;
}

void test3() {
    output_func_name();
}

Defer run(Defer &next){

    return newPromise([](Defer d){
        output_func_name();
        d.resolve();
    }).then([]() {
        output_func_name();
    }).then([](){
        output_func_name();
    }).then([&]()->Defer{
        output_func_name();
        next = newPromise([](Defer d) {
            output_func_name();
        });
        //Will call next.resole() or next.reject() later
        return next;
    }).then([]() -> int {
        output_func_name();
        return 5;
    }).fail([](){
        output_func_name();
    }).then(test1)
    .then(test2)
    .then(test3)
    .always([]() {
        output_func_name();
    });
}

Defer run2(Defer &next){

    return newPromise([](Defer d){
        output_func_name();
        d.resolve();
    }).then([]() {
        output_func_name();
    }).then([](){
        output_func_name();
    }).then([&]()->Defer{
        output_func_name();
        next = newPromise([](Defer d) {
            output_func_name();
        });
        //Will call next.resole() or next.reject() later
        return next;
    }).then([]() -> char {
        output_func_name();
        return 'c';
    }).fail([](){
        output_func_name();
    }).then(test1)
    .then(test2)
    .then(test3)
    .always([]() {
        output_func_name();
    });
}

void pm_run_loop(){
#define TT_SYSTICK_CLOCK        22118400
    pm_timer::init_system(TT_SYSTICK_CLOCK);
    while(true){
        __WFE();
        pm_timer::run();
        irq_x::run();
        defer_list::run();
    }
}


inline void LED_A(int on){
    if(on) P3->DOUT |= BIT6;
    else   P3->DOUT &= ~BIT6;
}

inline void LED_B(int on){
    if(on) P3->DOUT |= BIT7;
    else   P3->DOUT &= ~BIT7;    
}

Defer dd;

void test_0(int n){
    int *n_ = pm_new<int>(n);
    delay_ms(2000).then([]()->Defer {
        LED_B(1);
        return delay_ms(2000);
    }).then([=](){
        LED_B(0);
        if(*n_ > 0)
            test_0(*n_ - 1);
        else
            kill_timer(dd);
    }).bypass([=](){
        pm_delete(n_);
    });
}


void test_1(){
    dd = delay_ms(1000).then([]()->Defer {
        LED_A(1);
        dd = delay_ms(1000);
        return dd;
    }).then([]() {
        LED_A(0);
        test_1();
    });
}

void test_irq(){
    newPromise([](Defer d){
        LED_A(1);
        irq_disable();
        irq<SysTick_IRQn>::wait(d);
        irq_enable();
    }).then([]() {
        return newPromise([](Defer d){
            LED_A(0);
            irq_disable();
            irq<SysTick_IRQn>::wait(d);
            irq_enable();
        });
    }).then([]() {
        test_irq();
    });
}

void SysTick_Handler(){
    timer_global *global = pm_timer::get_global();
    global->current_ticks_++;
    
    if(global->current_ticks_ % 1024 == 0)
        irq<SysTick_IRQn>::post();
}

int main(){
    /* Open LED GPIO for testing */
    _GPIO_SET_PIN_MODE(LED0_GPIO_GRP, LED0_GPIO_BIT, GPIO_PMD_OUTPUT);
    _GPIO_SET_PIN_MODE(LED1_GPIO_GRP, LED1_GPIO_BIT, GPIO_PMD_OUTPUT);
    _GPIO_SET_PIN_MODE(LED2_GPIO_GRP, LED2_GPIO_BIT, GPIO_PMD_OUTPUT);
    _GPIO_SET_PIN_MODE(LED3_GPIO_GRP, LED3_GPIO_BIT, GPIO_PMD_OUTPUT);

    //test_0(2);
    test_1();
    Defer d;
    run(d);
    printf("after----\n");
    //d.resolve(3, 'c', 'd');
    d.resolve();
    
    //test_irq();
    pm_run_loop();
    return 0;
}
