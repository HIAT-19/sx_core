function(add_sx_library target_name)
    # 转发所有参数给 add_library
    add_library(${target_name} ${ARGN})

    # 获取目标类型
    get_target_property(target_type ${target_name} TYPE)

    # INTERFACE 库没有编译步骤，不需要设置编译选项
    if(NOT target_type STREQUAL "INTERFACE_LIBRARY")
        # 统一设置严格的编译告警选项
        target_compile_options(${target_name} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
            -pedantic
            -Wall
            -Wextra
            -Wconversion
            -Wdouble-promotion
            -Wshadow
            -Werror
            -Wno-unused-parameter
        )
    endif()

    # 自动创建别名 sx::name (如果 target_name 格式为 sx_name)
    # 例如：sx_infra -> sx::infra
    if(target_name MATCHES "^sx_(.+)$")
        set(alias_name "sx::${CMAKE_MATCH_1}")
        if(NOT TARGET ${alias_name})
            add_library(${alias_name} ALIAS ${target_name})
        endif()
    endif()
endfunction()

