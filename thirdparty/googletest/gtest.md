**gtest头文件:**

	#include "gtest/gtest.h"
	
	
**简单用例：**

	class_name:  类名/模块名 （自定义，不限制）
	method_name：方法名      （自定义，不限制）
	
	TEST(class_name, method_name) {
		ASSERT_XXX(): 断言 （断言失败会终止当前测试用例, 可以理解为失败就打印错误信息然后直接return）
		EXPECT_XXX()：期望  （期望失败不会终止当前测试用例，打印测试错误信息后继续执行）
	}
	
	TEST(environent_var, api_test) {
		ASSERT_EQ(env_set("os", "posix", 1), 0);
		ASSERT_EQ(env_set("path", "/home", 1), 0);
		ASSERT_EQ(env_set("ip", "192.168.1.1", 1), 0);

		ASSERT_STREQ(env_get("os"), "posix");
		ASSERT_STREQ(env_get("path"), "/home");
		ASSERT_STREQ(env_get("ip"), "192.168.1.1");
	}


**高级用例（需要稍微了解c++）：**

	1> 用户自定义测试类，需要继承Test基类
	class test_1 : public testing::Test {
	protected:
	
		void SetUp() override {          //虚函数, 在每个TEST_F 前执行
			//printf("SetUp: \n");
		}
		void TearDown() override {       //虚函数, 在每个TEST_F 返回时执行
			//printf("TearDown: \n");
		}
		
		static void SetUpTestCase() {    // 在TestCase前执行
			//printf("SetUpTestCase\n");
		}
		static void TearDownTestCase() { // 在TestCase返回时执行
			//printf("TearDownTestCase\n");
		}
	};	
	
	test_1:   类名（与自定义类名一致）
	method_1: 方法名（自定义，不限制）
	
	
	TEST_F(test_1, method_1) {
		//同上
	}
	
	TEST_F(test_1, method_2) {
		//同上
	}
	
	注意： 上面两个test 为同一个TestCase 
	



**基本断言：**

ASSERT_TRUE(condition);	    EXPECT_TRUE(condition);	    condition is true
ASSERT_FALSE(condition);	EXPECT_FALSE(condition);	condition is false

**二元断言：**

ASSERT_EQ(val1, val2);	EXPECT_EQ(val1, val2);	val1 == val2
ASSERT_NE(val1, val2);	EXPECT_NE(val1, val2);	val1 != val2
ASSERT_LT(val1, val2);	EXPECT_LT(val1, val2);	val1 < val2
ASSERT_LE(val1, val2);	EXPECT_LE(val1, val2);	val1 <= val2
ASSERT_GT(val1, val2);	EXPECT_GT(val1, val2);	val1 > val2
ASSERT_GE(val1, val2);	EXPECT_GE(val1, val2);	val1 >= val2


**字符串断言：**

ASSERT_STREQ(str1, str2);	    EXPECT_STREQ(str1, str2);	    the two C strings have the same content
ASSERT_STRNE(str1, str2);	    EXPECT_STRNE(str1, str2);	    the two C strings have different contents
ASSERT_STRCASEEQ(str1, str2);	EXPECT_STRCASEEQ(str1, str2);	the two C strings have the same content, ignoring case
ASSERT_STRCASENE(str1, str2);	EXPECT_STRCASENE(str1, str2);	the two C strings have different contents, ignoring case

