#pragma once
#include "jerzy_module.h"
extern "C" {
const JAModule *module_clone();
const JAModule *module_atomic();
const JAModule *module_attack();
const JAModule *module_tape();
const JAModule *module_mx();
const JAModule *module_memory();
const JAModule *module_reel();
const JAModule *module_oil();
}
