#pragma bank 255

#include <gbdk/platform.h>
#include "system.h"
#include "vm.h"
#include "gbs_types.h"
#include "events.h"
#include "input.h"
#include "math.h"
#include "simulate_input.h"

script_event_t input_sequence_event;
script_event_t input_sequence_completed_event;
UBYTE cancel_input_mask;

UBYTE sim_frame_joy;

void simulate_input_init(UBYTE preserve) BANKED {
    if (preserve) {
        input_sequence_completed_event.handle = 0;
		input_sequence_event.handle = 0;
    } else {
		cancel_input_mask = 0;
		input_sequence_completed_event.handle = 0;
		input_sequence_completed_event.script_addr = 0;
		input_sequence_completed_event.script_bank = 0;
		input_sequence_event.handle = 0;		
		input_sequence_event.script_addr = 0;		
		input_sequence_event.script_bank = 0;
    }
}

void simulate_input_update(void) NONBANKED {
    if (!input_sequence_event.script_addr) return;
	if (input_sequence_event.handle == 0){
		script_execute(input_sequence_event.script_bank, input_sequence_event.script_addr, &input_sequence_event.handle, 0);
	} else if ((input_sequence_event.handle & SCRIPT_TERMINATED) != 0) {
		input_sequence_event.script_addr = 0;
		input_sequence_event.script_bank = 0;
		input_sequence_event.handle = 0;
		if (input_sequence_completed_event.script_addr){
			script_execute(input_sequence_completed_event.script_bank, input_sequence_completed_event.script_addr, &input_sequence_event.handle, 0);
		}
	} else if (joy & cancel_input_mask) {
		script_terminate(input_sequence_event.handle);
	} else {
		joy = sim_frame_joy;
	}
}

void vm_attach_simulate_input(SCRIPT_CTX * THIS) OLDCALL BANKED {
	cancel_input_mask = *(int8_t*)VM_REF_TO_PTR(FN_ARG0);
	uint8_t input_sequence_bank = *(uint8_t *) VM_REF_TO_PTR(FN_ARG1);
	void* input_sequence_ptr = *(void**) VM_REF_TO_PTR(FN_ARG2);	
	uint8_t input_sequence_completed_bank = *(uint8_t *) VM_REF_TO_PTR(FN_ARG3);
	void* input_sequence_completed_ptr = *(void**) VM_REF_TO_PTR(FN_ARG4);

    input_sequence_event.script_bank = input_sequence_bank;
    input_sequence_event.script_addr = input_sequence_ptr;	
	input_sequence_completed_event.script_bank = input_sequence_completed_bank;
    input_sequence_completed_event.script_addr = input_sequence_completed_ptr;
}

void vm_set_simulated_input(SCRIPT_CTX * THIS) OLDCALL BANKED {
	sim_frame_joy = *(int8_t*)VM_REF_TO_PTR(FN_ARG0);	
}