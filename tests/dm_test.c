/**
 * @file dm_test.c
 * @author Rastislav Szabo <raszabo@cisco.com>, Lukas Macko <lmacko@cisco.com>
 * @brief Data Manager unit tests.
 *
 * @copyright
 * Copyright 2015 Cisco Systems, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include <stdio.h>
#include "data_manager.h"
#include "test_data.h"
#include "sr_common.h"
#include "test_module_helper.h"

int setup(void **state)
{
    /* make sure that test-module data is created */
    createDataTreeTestModule();
    return 0;
}

void dm_create_cleanup(void **state){
   int rc;
   dm_ctx_t *ctx;
   rc = dm_init(TEST_SCHEMA_SEARCH_DIR, TEST_DATA_SEARCH_DIR, &ctx);
   assert_int_equal(SR_ERR_OK,rc);

   dm_cleanup(ctx);

}

void dm_get_data_tree(void **state)
{
    int rc;
    dm_ctx_t *ctx;
    dm_session_t *ses_ctx;
    struct lyd_node *data_tree;

    rc = dm_init(TEST_SCHEMA_SEARCH_DIR, TEST_DATA_SEARCH_DIR, &ctx);
    assert_int_equal(SR_ERR_OK, rc);

    dm_session_start(ctx, SR_DS_STARTUP, &ses_ctx);
    /* Load from file */
    assert_int_equal(SR_ERR_OK, dm_get_datatree(ctx, ses_ctx ,"example-module", &data_tree));
    /* Get from avl tree */
    assert_int_equal(SR_ERR_OK, dm_get_datatree(ctx, ses_ctx ,"example-module", &data_tree));
    /* Module without data*/
    assert_int_equal(SR_ERR_NOT_FOUND, dm_get_datatree(ctx, ses_ctx ,"small-module", &data_tree));
    /* Not existing module should return an error*/
    assert_int_equal(SR_ERR_UNKNOWN_MODEL, dm_get_datatree(ctx, ses_ctx ,"not-existing-module", &data_tree));

    dm_session_stop(ctx, ses_ctx);

    dm_cleanup(ctx);

}

void
dm_list_schema_test(void **state)
{
    int rc;
    dm_ctx_t *ctx;
    dm_session_t *ses_ctx;
    sr_schema_t *schemas;
    size_t count;

    rc = dm_init(TEST_SCHEMA_SEARCH_DIR, TEST_DATA_SEARCH_DIR, &ctx);
    assert_int_equal(SR_ERR_OK, rc);

    dm_session_start(ctx, SR_DS_STARTUP, &ses_ctx);

    rc = dm_list_schemas(ctx, ses_ctx, &schemas, &count);
    assert_int_equal(SR_ERR_OK, rc);

    for (size_t i = 0; i < count; i++) {
        printf("\n\nSchema #%zu:\n%s\n%s\n%s\n%s\n%s", i,
                schemas[i].module_name,
                schemas[i].ns,
                schemas[i].prefix,
                schemas[i].revision,
                schemas[i].file_path);
    }

    sr_free_schemas(schemas, count);

    dm_session_stop(ctx, ses_ctx);

    dm_cleanup(ctx);
}

void
dm_validate_data_trees_test(void **state)
{
    int rc;
    dm_ctx_t *ctx = NULL;
    dm_session_t *ses_ctx = NULL;
    struct lyd_node *node = NULL;
    dm_data_info_t *info = NULL;
    sr_error_info_t *errors = NULL;
    size_t err_cnt = 0;

    rc = dm_init(TEST_SCHEMA_SEARCH_DIR, TEST_DATA_SEARCH_DIR, &ctx);
    assert_int_equal(SR_ERR_OK, rc);

    rc = dm_session_start(ctx, SR_DS_STARTUP, &ses_ctx);
    assert_int_equal(SR_ERR_OK, rc);

    /* test validation with no data trees copied */
    rc = dm_validate_session_data_trees(ctx, ses_ctx, &errors, &err_cnt);
    assert_int_equal(SR_ERR_OK, rc);
    sr_free_errors(errors, err_cnt);

    /* copy a couple data trees to session*/
    rc = dm_get_data_info(ctx, ses_ctx, "example-module", &info);
    assert_int_equal(SR_ERR_OK, rc);

    rc = dm_get_data_info(ctx, ses_ctx, "test-module", &info);
    assert_int_equal(SR_ERR_OK, rc);

    rc = dm_validate_session_data_trees(ctx, ses_ctx, &errors, &err_cnt);
    assert_int_equal(SR_ERR_OK, rc);
    sr_free_errors(errors, err_cnt);

    /* make an invalid  change */
    info->modified = true;
    /* already existing leaf */
    node = sr_lyd_new_leaf(info, info->node, info->module, "i8", "42");
    assert_non_null(node);


    rc = dm_validate_session_data_trees(ctx, ses_ctx, &errors, &err_cnt);
    assert_int_equal(SR_ERR_VALIDATION_FAILED, rc);
    sr_free_errors(errors, err_cnt);

    dm_session_stop(ctx, ses_ctx);
    dm_cleanup(ctx);
}

void
dm_discard_changes_test(void **state)
{
    int rc;
    dm_ctx_t *ctx = NULL;
    dm_session_t *ses_ctx = NULL;
    dm_data_info_t *info = NULL;

    rc = dm_init(TEST_SCHEMA_SEARCH_DIR, TEST_DATA_SEARCH_DIR, &ctx);
    assert_int_equal(SR_ERR_OK, rc);

    rc = dm_session_start(ctx, SR_DS_STARTUP, &ses_ctx);
    assert_int_equal(SR_ERR_OK, rc);

    rc = dm_get_data_info(ctx, ses_ctx, "test-module", &info);
    assert_int_equal(SR_ERR_OK, rc);

    rc = dm_discard_changes(ctx, ses_ctx);
    assert_int_equal(SR_ERR_OK, rc);

    rc = dm_get_data_info(ctx, ses_ctx, "test-module", &info);
    assert_int_equal(SR_ERR_OK, rc);

    /* check current value */
    assert_int_equal(8, ((struct lyd_node_leaf_list *)info->node->child->next->next->next->next)->value.int8);


    /* change leaf i8 value */
    info->modified = true;
    //TODO change to lyd_change_leaf
    ((struct lyd_node_leaf_list *)info->node->child->next->next->next->next)->value.int8 = 100;

    /* we should have the value changed*/
    rc = dm_get_data_info(ctx, ses_ctx, "test-module", &info);
    assert_int_equal(SR_ERR_OK, rc);

    assert_int_equal(100, ((struct lyd_node_leaf_list *)info->node->child->next->next->next->next)->value.int8);

    /* discard changes to get current datastore value*/
    rc = dm_discard_changes(ctx, ses_ctx);
    assert_int_equal(SR_ERR_OK, rc);

    rc = dm_get_data_info(ctx, ses_ctx, "test-module", &info);
    assert_int_equal(SR_ERR_OK, rc);

    assert_int_equal(8, ((struct lyd_node_leaf_list *)info->node->child->next->next->next->next)->value.int8);

    dm_session_stop(ctx, ses_ctx);
    dm_cleanup(ctx);
}

void
dm_commit_test(void **state)
{
    int rc;
    dm_ctx_t *ctx = NULL;
    dm_session_t *ses_ctx = NULL;
    dm_data_info_t *info = NULL;

    rc = dm_init(TEST_SCHEMA_SEARCH_DIR, TEST_DATA_SEARCH_DIR, &ctx);
    assert_int_equal(SR_ERR_OK, rc);

    rc = dm_session_start(ctx, SR_DS_STARTUP, &ses_ctx);
    assert_int_equal(SR_ERR_OK, rc);

    sr_error_info_t *errors = NULL;
    size_t err_cnt = 0;
    rc = dm_commit(ctx, ses_ctx, &errors, &err_cnt);
    assert_int_equal(SR_ERR_OK, rc);
    sr_free_errors(errors, err_cnt);

    rc = dm_get_data_info(ctx, ses_ctx, "test-module", &info);
    assert_int_equal(SR_ERR_OK, rc);

    rc = dm_commit(ctx, ses_ctx, &errors, &err_cnt);
    assert_int_equal(SR_ERR_OK, rc);
    sr_free_errors(errors, err_cnt);

    rc = dm_get_data_info(ctx, ses_ctx, "test-module", &info);
    assert_int_equal(SR_ERR_OK, rc);
    info->modified = true;

    rc = dm_commit(ctx, ses_ctx, &errors, &err_cnt);
    assert_int_equal(SR_ERR_OK, rc);
    sr_free_errors(errors, err_cnt);

    dm_session_stop(ctx, ses_ctx);
    dm_cleanup(ctx);
}

void
dm_add_operation_test(void **state)
{
    int rc;
    dm_ctx_t *ctx = NULL;
    dm_session_t *ses_ctx = NULL;

    dm_session_start(ctx, SR_DS_STARTUP, &ses_ctx);

    rc = dm_add_operation(ses_ctx, DM_DELETE_OP, NULL, NULL, SR_EDIT_DEFAULT);
    assert_int_equal(SR_ERR_INVAL_ARG, rc);

    sr_val_t *val = NULL;
    val = calloc(1, sizeof(*val));
    assert_null(val);

    val->type = SR_INT8_T;
    val->data.int8_val = 42;

    xp_loc_id_t *l1 = NULL;
    xp_loc_id_t *l2 = NULL;

    assert_int_equal(SR_ERR_OK, xp_char_to_loc_id("/abc:def", &l1));
    assert_int_equal(SR_ERR_OK, xp_char_to_loc_id("/abc:def", &l2));

    rc = dm_add_operation(ses_ctx, DM_SET_OP, l1, val, SR_EDIT_DEFAULT);
    assert_int_equal(SR_ERR_OK, rc);

    rc = dm_add_operation(ses_ctx, DM_DELETE_OP, l2, NULL, SR_EDIT_DEFAULT);
    assert_int_equal(SR_ERR_OK, rc);

    dm_session_stop(ctx, ses_ctx);

}

int main(){
    sr_set_log_level(SR_LL_DBG, SR_LL_NONE);

    const struct CMUnitTest tests[] = {
            cmocka_unit_test(dm_create_cleanup),
            cmocka_unit_test(dm_get_data_tree),
            cmocka_unit_test(dm_list_schema_test),
            cmocka_unit_test(dm_list_schema_test),
            cmocka_unit_test(dm_validate_data_trees_test),
            cmocka_unit_test(dm_discard_changes_test),
            cmocka_unit_test(dm_commit_test),
    };
    return cmocka_run_group_tests(tests, setup, NULL);
}

