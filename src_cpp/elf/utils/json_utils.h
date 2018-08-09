/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#define JSON_LOAD(target, j, field)                          \
  if (j.find(#field) != j.end()) {                           \
    target.field = j[#field];                                \
  } else {                                                   \
    throw std::runtime_error(#field "cannot not be found!"); \
  }

#define JSON_LOAD_OPTIONAL(target, j, field) \
  if (j.find(#field) != j.end()) {           \
    target.field = j[#field];                \
  }

#define JSON_SAVE(j, field) j[#field] = field;

#define JSON_SAVE_OBJ(j, field) field.setJsonFields(j[#field]);

#define JSON_LOAD_OBJ(target, j, field)                      \
  if (j.find(#field) != j.end()) {                           \
    target.field = target.field.createFromJson(j[#field]);   \
  } else {                                                   \
    throw std::runtime_error(#field "cannot not be found!"); \
  }

#define JSON_LOAD_OBJ_ARGS(target, j, field, ...)                       \
  if (j.find(#field) != j.end()) {                                      \
    target.field = target.field.createFromJson(j[#field], __VA_ARGS__); \
  } else {                                                              \
    throw std::runtime_error(#field "cannot not be found!");            \
  }

#define JSON_LOAD_VEC(target, j, field)                      \
  target.field.clear();                                      \
  if (j.find(#field) != j.end()) {                           \
    for (size_t i = 0; i < j[#field].size(); ++i) {          \
      target.field.push_back(j[#field][i]);                  \
    }                                                        \
  } else {                                                   \
    throw std::runtime_error(#field "cannot not be found!"); \
  }

#define JSON_LOAD_VEC_OPTIONAL(target, j, field)    \
  target.field.clear();                             \
  if (j.find(#field) != j.end()) {                  \
    for (size_t i = 0; i < j[#field].size(); ++i) { \
      target.field.push_back(j[#field][i]);         \
    }                                               \
  }
