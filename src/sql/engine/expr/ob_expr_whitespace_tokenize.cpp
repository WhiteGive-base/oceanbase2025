/**
 * Copyright (c) 2024 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#define USING_LOG_PREFIX SQL_ENG

#include "sql/engine/expr/ob_expr_whitespace_tokenize.h"

#define N_WHITESPACE_TOKENIZE "whitespace_tokenize"
#include "lib/charset/ob_charset.h"
#include "lib/json_type/ob_json_base.h"
#include "lib/json_type/ob_json_tree.h"
#include "lib/ob_errno.h"
#include "lib/oblog/ob_log_module.h"
#include "lib/string/ob_string.h"
#include "sql/engine/expr/ob_expr_json_func_helper.h"

namespace oceanbase
{
namespace sql
{

ObExprWhitespaceTokenize::ObExprWhitespaceTokenize(common::ObIAllocator &alloc)
    : ObStringExprOperator(alloc,
                           T_FUN_WHITESPACE_TOKENIZE,
                           N_WHITESPACE_TOKENIZE,
                           1,
                           VALID_FOR_GENERATED_COL)
{
}

ObExprWhitespaceTokenize::~ObExprWhitespaceTokenize() {}

int ObExprWhitespaceTokenize::eval_whitespace_tokenize(const ObExpr &expr, ObEvalCtx &ctx, ObDatum &expr_datum)
{
  int ret = OB_SUCCESS;
  ObDatum *text_datum = nullptr;
  
  if (OB_FAIL(expr.args_[0]->eval(ctx, text_datum))) {
    LOG_WARN("Fail to eval text parameter", K(ret));
  } else if (text_datum->is_null()) {
    // NULL input returns NULL
    expr_datum.set_null();
  } else {
    ObEvalCtx::TempAllocGuard tmp_alloc_g(ctx);
    common::ObArenaAllocator &temp_allocator = tmp_alloc_g.get_allocator();
    ObString text = text_datum->get_string();
    ObIJsonBase *json_result = nullptr;
    
    // Create JSON array for result
    void *buf = temp_allocator.alloc(sizeof(ObJsonArray));
    if (OB_ISNULL(buf)) {
      ret = OB_ALLOCATE_MEMORY_FAILED;
      LOG_WARN("Failed to allocate memory for JSON array", K(ret));
    } else {
      ObJsonArray *json_array = new (buf) ObJsonArray(&temp_allocator);
      
      // Tokenize by whitespace
      const char *ptr = text.ptr();
      int32_t len = text.length();
      int32_t start = 0;
      
      for (int32_t i = 0; i <= len && OB_SUCC(ret); ++i) {
        bool is_whitespace = (i < len && (ptr[i] == ' ' || ptr[i] == '\t' || 
                                          ptr[i] == '\n' || ptr[i] == '\r'));
        bool is_end = (i == len);
        
        if ((is_whitespace || is_end) && i > start) {
          // Found a token
          ObString token(i - start, ptr + start);
          
          // Create JSON string for this token
          void *token_buf = temp_allocator.alloc(sizeof(ObJsonString));
          if (OB_ISNULL(token_buf)) {
            ret = OB_ALLOCATE_MEMORY_FAILED;
            LOG_WARN("Failed to allocate memory for JSON string", K(ret));
          } else {
            ObJsonString *json_str = new (token_buf) ObJsonString(token);
            if (OB_FAIL(json_array->append(json_str))) {
              LOG_WARN("Failed to append token to array", K(ret));
            }
          }
          start = i + 1;
        } else if (is_whitespace) {
          start = i + 1;
        }
      }
      
      if (OB_SUCC(ret)) {
        json_result = json_array;
        if (OB_FAIL(ObJsonExprHelper::pack_json_res(expr,
                                                    ctx,
                                                    temp_allocator,
                                                    json_result,
                                                    expr_datum))) {
          LOG_WARN("Failed to pack JSON result", K(ret));
        }
      }
    }
  }
  
  return ret;
}

int ObExprWhitespaceTokenize::calc_result_type1(ObExprResType &type,
                                                 ObExprResType &type1,
                                                 common::ObExprTypeCtx &type_ctx) const
{
  int ret = OB_SUCCESS;
  
  if (lib::is_oracle_mode()) {
    ret = OB_NOT_IMPLEMENT;
    LOG_WARN("whitespace_tokenize not supported in Oracle mode", K(ret));
  } else {
    // Set result type to JSON
    type.set_json();
    type.set_length(static_cast<ObLength>(ObAccuracy::DDL_DEFAULT_ACCURACY[ObJsonType].get_length()));
    
    // Set input parameter type expectations
    if (ob_is_string_type(type1.get_type())) {
      if (type1.get_charset_type() != CHARSET_UTF8MB4) {
        type1.set_calc_collation_type(CS_TYPE_UTF8MB4_BIN);
      }
    }
  }
  
  return ret;
}

int ObExprWhitespaceTokenize::cg_expr(ObExprCGCtx &op_cg_ctx,
                                      const ObRawExpr &raw_expr,
                                      ObExpr &rt_expr) const
{
  int ret = OB_SUCCESS;
  UNUSED(op_cg_ctx);
  UNUSED(raw_expr);
  
  if (rt_expr.arg_cnt_ != 1) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("Invalid argument count", K(ret), K(rt_expr.arg_cnt_));
  } else {
    rt_expr.eval_func_ = eval_whitespace_tokenize;
  }
  
  return ret;
}

} // namespace sql
} // namespace oceanbase

