#!/usr/bin/env python3
#  Copyright 2016 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS-IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from fruit_test_common import *

COMMON_DEFINITIONS = '''
    #include "test_common.h"

    struct X;

    struct Annotation1 {};
    using XAnnot1 = fruit::Annotated<Annotation1, X>;
    '''

@pytest.mark.parametrize('XParamInChildComponent,XParamInRootComponent', [
    ('X', 'X'),
    ('X', 'const X'),
    ('fruit::Annotated<Annotation1, X>', 'fruit::Annotated<Annotation1, X>'),
    ('fruit::Annotated<Annotation1, X>', 'fruit::Annotated<Annotation1, const X>'),
])
def test_success(XParamInChildComponent, XParamInRootComponent):
    source = '''
        struct X {
          int n;
          X(int n) : n(n) {}
        };

        fruit::Component<XParamInChildComponent> getChildComponent() {
          return fruit::createComponent()
            .registerProvider<XParamInChildComponent()>([]() { return X(5); });
        }

        fruit::Component<XParamInRootComponent> getRootComponent() {
          return fruit::createComponent()
            .install(getChildComponent);
        }

        int main() {
          fruit::Injector<XParamInRootComponent> injector(getRootComponent);
          X x = injector.get<XParamInRootComponent>();
          Assert(x.n == 5);
        }
        '''
    expect_success(
        COMMON_DEFINITIONS,
        source,
        locals())

@pytest.mark.parametrize('XParamInChildComponent,XParamInRootComponent', [
    ('const X', 'X'),
    ('fruit::Annotated<Annotation1, const X>', 'fruit::Annotated<Annotation1, X>'),
])
def test_install_error_child_component_provides_const(XParamInChildComponent, XParamInRootComponent):
    source = '''
        struct X {};

        fruit::Component<XParamInChildComponent> getChildComponent();

        fruit::Component<XParamInRootComponent> getRootComponent() {
          return fruit::createComponent()
            .install(getChildComponent);
        }
        '''
    expect_compile_error(
        'NonConstBindingRequiredButConstBindingProvidedError<XParamInRootComponent>',
        'The type T was provided as constant, however one of the constructors/providers/factories in this component',
        COMMON_DEFINITIONS,
        source,
        locals())

@pytest.mark.parametrize('ProvidedXParam,RequiredXParam', [
    ('X', 'X'),
    ('X', 'const X'),
    ('const X', 'const X'),
    ('fruit::Annotated<Annotation1, X>', 'fruit::Annotated<Annotation1, X>'),
    ('fruit::Annotated<Annotation1, X>', 'fruit::Annotated<Annotation1, const X>'),
    ('fruit::Annotated<Annotation1, const X>', 'fruit::Annotated<Annotation1, const X>'),
])
def test_with_requirements_success(ProvidedXParam, RequiredXParam):
    ProvidedXParamWithoutConst = ProvidedXParam.replace('const ', '')
    source = '''
        struct X {
          int n;
          X(int n) : n(n) {}
        };

        struct Y {
          X x;
          Y(X x): x(x) {}
        };

        fruit::Component<fruit::Required<RequiredXParam>, Y> getChildComponent1() {
          return fruit::createComponent()
            .registerProvider<Y(RequiredXParam)>([](X x) { return Y(x); });
        }

        fruit::Component<ProvidedXParam> getChildComponent2() {
          return fruit::createComponent()
            .registerProvider<ProvidedXParamWithoutConst()>([]() { return X(5); });
        }

        fruit::Component<Y> getRootComponent() {
          return fruit::createComponent()
            .install(getChildComponent1)
            .install(getChildComponent2);
        }

        int main() {
          fruit::Injector<Y> injector(getRootComponent);
          Y y = injector.get<Y>();
          Assert(y.x.n == 5);
        }
        '''
    expect_success(
        COMMON_DEFINITIONS,
        source,
        locals())

@pytest.mark.parametrize('ProvidedXParam,RequiredXParam', [
    ('const X', 'X'),
    ('fruit::Annotated<Annotation1, const X>', 'fruit::Annotated<Annotation1, X>'),
])
def test_with_requirements_error_only_nonconst_provided(ProvidedXParam, RequiredXParam):
    source = '''
        struct X {};
        struct Y {};

        fruit::Component<fruit::Required<RequiredXParam>, Y> getChildComponent1();

        fruit::Component<ProvidedXParam> getChildComponent2();

        fruit::Component<Y> getRootComponent() {
          return fruit::createComponent()
            .install(getChildComponent1)
            .install(getChildComponent2);
        }
        '''
    expect_compile_error(
        'NonConstBindingRequiredButConstBindingProvidedError<RequiredXParam>',
        'The type T was provided as constant, however one of the constructors/providers/factories in this component',
        COMMON_DEFINITIONS,
        source,
        locals())

@pytest.mark.parametrize('ProvidedXParam,RequiredXParam', [
    ('const X', 'X'),
    ('fruit::Annotated<Annotation1, const X>', 'fruit::Annotated<Annotation1, X>'),
])
def test_with_requirements_error_only_nonconst_provided_reversed_install_order(ProvidedXParam, RequiredXParam):
    source = '''
        struct X {};
        struct Y {};

        fruit::Component<fruit::Required<RequiredXParam>, Y> getChildComponent1();

        fruit::Component<ProvidedXParam> getChildComponent2();

        fruit::Component<Y> getRootComponent() {
          return fruit::createComponent()
            .install(getChildComponent2)
            .install(getChildComponent1);
        }
        '''
    expect_compile_error(
        'NonConstBindingRequiredButConstBindingProvidedError<RequiredXParam>',
        'The type T was provided as constant, however one of the constructors/providers/factories in this component',
        COMMON_DEFINITIONS,
        source,
        locals())

def test_with_requirements_not_specified_in_child_component_error():
    source = '''
        struct X {
          int n;
          X(int n) : n(n) {}
        };

        struct Y {
          X x;
          Y(X x): x(x) {}
        };

        fruit::Component<fruit::Required<X>, Y> getParentYComponent() {
          return fruit::createComponent()
            .registerProvider([](X x) { return Y(x); });
        }

        // We intentionally don't have fruit::Required<X> here, we want to test that this results in an error.
        fruit::Component<Y> getYComponent() {
          return fruit::createComponent()
            .install(getParentYComponent);
        }
        '''
    expect_compile_error(
        'NoBindingFoundError<X>',
        'No explicit binding nor C::Inject definition was found for T',
        COMMON_DEFINITIONS,
        source)

@pytest.mark.parametrize('XAnnot,ConstXAnnot', [
    ('X', 'const X'),
    ('fruit::Annotated<Annotation1, X>', 'fruit::Annotated<Annotation1, const X>'),
])
def test_install_requiring_nonconst_then_install_requiring_const_ok(XAnnot, ConstXAnnot):
    source = '''
        struct X {};
        struct Y {};
        struct Z {};

        fruit::Component<fruit::Required<XAnnot>, Y> getChildComponent1() {
          return fruit::createComponent()
              .registerConstructor<Y()>();
        }
        
        fruit::Component<fruit::Required<ConstXAnnot>, Z> getChildComponent2() {
          return fruit::createComponent()
              .registerConstructor<Z()>();
        }

        fruit::Component<Y, Z> getRootComponent() {
          return fruit::createComponent()
            .install(getChildComponent1)
            .install(getChildComponent2)
            .registerConstructor<XAnnot()>();
        }
        
        int main() {
          fruit::Injector<Y, Z> injector(getRootComponent);
          injector.get<Y>();
          injector.get<Z>();
        }
        '''
    expect_success(
        COMMON_DEFINITIONS,
        source,
        locals())

def test_install_requiring_nonconst_then_install_requiring_const_declaring_const_requirement_error():
    source = '''
        struct X {};
        struct Y {};
        struct Z {};

        fruit::Component<fruit::Required<X>, Y> getChildComponent1();
        fruit::Component<fruit::Required<const X>, Z> getChildComponent2();

        fruit::Component<fruit::Required<const X>, Y, Z> getRootComponent() {
          return fruit::createComponent()
            .install(getChildComponent1)
            .install(getChildComponent2);
        }
        '''
    expect_compile_error(
        'ConstBindingDeclaredAsRequiredButNonConstBindingRequiredError<X>',
        'The type T was declared as a const Required type in the returned Component, however',
        COMMON_DEFINITIONS,
        source,
        locals())

def test_install_requiring_const_then_install_requiring_nonconst_ok():
    source = '''
        struct X {};
        struct Y {};
        struct Z {};

        fruit::Component<fruit::Required<const X>, Y> getChildComponent1() {
          return fruit::createComponent()
              .registerConstructor<Y()>();
        }
        
        fruit::Component<fruit::Required<X>, Z> getChildComponent2() {
          return fruit::createComponent()
              .registerConstructor<Z()>();
        }

        fruit::Component<Y, Z> getRootComponent() {
          return fruit::createComponent()
            .install(getChildComponent1)
            .install(getChildComponent2)
            .registerConstructor<X()>();
        }
        
        int main() {
          fruit::Injector<Y, Z> injector(getRootComponent);
          injector.get<Y>();
          injector.get<Z>();
        }
        '''
    expect_success(
        COMMON_DEFINITIONS,
        source,
        locals())

def test_install_requiring_const_then_install_requiring_nonconst_declaring_const_requirement_error():
    source = '''
        struct X {};
        struct Y {};
        struct Z {};

        fruit::Component<fruit::Required<const X>, Y> getChildComponent1();
        fruit::Component<fruit::Required<X>, Z> getChildComponent2();

        fruit::Component<fruit::Required<const X>, Y, Z> getRootComponent() {
          return fruit::createComponent()
            .install(getChildComponent1)
            .install(getChildComponent2);
        }
        '''
    expect_compile_error(
        'ConstBindingDeclaredAsRequiredButNonConstBindingRequiredError<X>',
        'The type T was declared as a const Required type in the returned Component, however',
        COMMON_DEFINITIONS,
        source,
        locals())

def test_install_with_args_success():
    source = '''
        struct X {
          int n;
          X(int n) : n(n) {}
        };
        
        struct Arg {
          Arg(int) {}
          Arg() = default;
          Arg(const Arg&) = default;
          Arg(Arg&&) = default;
          Arg& operator=(const Arg&) = default;
          Arg& operator=(Arg&&) = default;
        };
        
        bool operator==(const Arg&, const Arg&) {
          return true;
        }
        
        namespace std {
          template <>
          struct hash<Arg> {
            size_t operator()(const Arg&) {
              return 0;
            }
          };
        }

        fruit::Component<X> getParentComponent(int, std::string, Arg, Arg) {
          return fruit::createComponent()
            .registerProvider([]() { return X(5); });
        }

        fruit::Component<X> getComponent() {
          return fruit::createComponent()
            .install(getParentComponent, 5, std::string("Hello"), Arg{}, 15);
        }

        int main() {
          fruit::Injector<X> injector(getComponent);
          X x = injector.get<X>();
          Assert(x.n == 5);
        }
        '''
    expect_success(COMMON_DEFINITIONS, source)

def test_install_with_args_error_not_move_constructible():
    source = '''
        struct Arg {
          Arg() = default;
          Arg(const Arg&) = default;
          Arg(Arg&&) = delete;
          Arg& operator=(const Arg&) = default;
          Arg& operator=(Arg&&) = default;
        };
        
        bool operator==(const Arg&, const Arg&);
        
        namespace std {
          template <>
          struct hash<Arg> {
            size_t operator()(const Arg&);
          };
        }

        fruit::Component<X> getParentComponent(int, std::string, Arg);

        fruit::Component<X> getComponent() {
          return fruit::createComponent()
            .install(getParentComponent, 5, std::string("Hello"), Arg{});
        }
        '''
    expect_generic_compile_error(
        'error: use of deleted function .Arg::Arg\(Arg&&\).'
            + '|error: call to deleted constructor of .Arg.'
            + '|error C2280: .Arg::Arg\(Arg &&\).: attempting to reference a deleted function',
        COMMON_DEFINITIONS,
        source)

def test_install_with_args_error_not_move_constructible_with_conversion():
    source = '''
        struct Arg {
          Arg(int) {}
          Arg() = default;
          Arg(const Arg&) = default;
          Arg(Arg&&) = delete;
          Arg& operator=(const Arg&) = default;
          Arg& operator=(Arg&&) = default;
        };
        
        bool operator==(const Arg&, const Arg&);
        
        namespace std {
          template <>
          struct hash<Arg> {
            size_t operator()(const Arg&);
          };
        }

        fruit::Component<X> getParentComponent(int, std::string, Arg);

        fruit::Component<X> getComponent() {
          return fruit::createComponent()
            .install(getParentComponent, 5, std::string("Hello"), 15);
        }
        '''
    expect_generic_compile_error(
        'error: use of deleted function .Arg::Arg\(Arg&&\).'
            + '|error: call to deleted constructor of .Arg.'
            + '|error C2280: .Arg::Arg\(Arg &&\).: attempting to reference a deleted function',
        COMMON_DEFINITIONS,
        source)

def test_install_with_args_error_not_copy_constructible():
    source = '''
        struct X {
          int n;
          X(int n) : n(n) {}
        };
        
        struct Arg {
          Arg() = default;
          Arg(const Arg&) = delete;
          Arg(Arg&&) = default;
          Arg& operator=(const Arg&) = default;
          Arg& operator=(Arg&&) = default;
        };
        
        bool operator==(const Arg&, const Arg&);
        
        namespace std {
          template <>
          struct hash<Arg> {
            size_t operator()(const Arg&);
          };
        }

        fruit::Component<X> getParentComponent(int, std::string, Arg);

        fruit::Component<X> getComponent() {
          return fruit::createComponent()
            .install(getParentComponent, 5, std::string("Hello"), Arg{});
        }
        '''
    expect_generic_compile_error(
        'error: use of deleted function .Arg::Arg\(const Arg&\).'
            + '|error: call to deleted constructor of .Arg.'
            + '|error C2280: .Arg::Arg\(const Arg &\).: attempting to reference a deleted function',
        COMMON_DEFINITIONS,
        source)

def test_install_with_args_error_not_copy_constructible_with_conversion():
    source = '''
        struct X {
          int n;
          X(int n) : n(n) {}
        };
        
        struct Arg {
          Arg(int) {}
          Arg() = default;
          Arg(const Arg&) = delete;
          Arg(Arg&&) = default;
          Arg& operator=(const Arg&) = default;
          Arg& operator=(Arg&&) = default;
        };
        
        bool operator==(const Arg&, const Arg&);
        
        namespace std {
          template <>
          struct hash<Arg> {
            size_t operator()(const Arg&);
          };
        }

        fruit::Component<X> getParentComponent(int, std::string, Arg);

        fruit::Component<X> getComponent() {
          return fruit::createComponent()
            .install(getParentComponent, 5, std::string("Hello"), 15);
        }
        '''
    expect_generic_compile_error(
        'error: use of deleted function .Arg::Arg\(const Arg&\).'
            + '|error: call to deleted constructor of .Arg.'
            + '|error C2280: .Arg::Arg\(const Arg &\).: attempting to reference a deleted function',
        COMMON_DEFINITIONS,
        source)

def test_install_with_args_error_not_move_assignable():
    source = '''
        struct Arg {
          Arg() = default;
          Arg(const Arg&) = default;
          Arg(Arg&&) = default;
          Arg& operator=(const Arg&) = default;
          Arg& operator=(Arg&&) = delete;
        };
        
        bool operator==(const Arg&, const Arg&);
        
        namespace std {
          template <>
          struct hash<Arg> {
            size_t operator()(const Arg&);
          };
        }

        fruit::Component<X> getParentComponent(int, std::string, Arg);

        fruit::Component<X> getComponent() {
          return fruit::createComponent()
            .install(getParentComponent, 5, std::string("Hello"), Arg{});
        }
        '''
    expect_generic_compile_error(
        'error: use of deleted function .Arg& Arg::operator=\(Arg&&\).'
            + '|error: overload resolution selected deleted operator .=.'
            + '|error C2280: .Arg &Arg::operator =\(Arg &&\).: attempting to reference a deleted function',
        COMMON_DEFINITIONS,
        source)

def test_install_with_args_error_not_move_assignable_with_conversion():
    source = '''
        struct Arg {
          Arg(int) {}
          Arg() = default;
          Arg(const Arg&) = default;
          Arg(Arg&&) = default;
          Arg& operator=(const Arg&) = default;
          Arg& operator=(Arg&&) = delete;
        };
        
        bool operator==(const Arg&, const Arg&);
        
        namespace std {
          template <>
          struct hash<Arg> {
            size_t operator()(const Arg&);
          };
        }

        fruit::Component<X> getParentComponent(int, std::string, Arg);

        fruit::Component<X> getComponent() {
          return fruit::createComponent()
            .install(getParentComponent, 5, std::string("Hello"), 15);
        }
        '''
    expect_generic_compile_error(
        'error: use of deleted function .Arg& Arg::operator=\(Arg&&\).'
            + '|error: overload resolution selected deleted operator .=.'
            + '|error C2280: .Arg &Arg::operator =\(Arg &&\).: attempting to reference a deleted function',
        COMMON_DEFINITIONS,
        source)

def test_install_with_args_error_not_copy_assignable():
    source = '''
        struct X {
          int n;
          X(int n) : n(n) {}
        };
        
        struct Arg {
          Arg() = default;
          Arg(const Arg&) = default;
          Arg(Arg&&) = default;
          Arg& operator=(const Arg&) = delete;
          Arg& operator=(Arg&&) = default;
        };
        
        bool operator==(const Arg&, const Arg&);
        
        namespace std {
          template <>
          struct hash<Arg> {
            size_t operator()(const Arg&);
          };
        }

        fruit::Component<X> getParentComponent(int, std::string, Arg);

        fruit::Component<X> getComponent() {
          return fruit::createComponent()
            .install(getParentComponent, 5, std::string("Hello"), Arg{});
        }
        '''
    expect_generic_compile_error(
        'error: use of deleted function .Arg& Arg::operator=\(const Arg&\).'
            + '|error: overload resolution selected deleted operator .=.'
            + '|error C2280: .Arg &Arg::operator =\(const Arg &\).: attempting to reference a deleted function',
        COMMON_DEFINITIONS,
        source)

def test_install_with_args_error_not_copy_assignable_with_conversion():
    source = '''
        struct X {
          int n;
          X(int n) : n(n) {}
        };
        
        struct Arg {
          Arg(int) {}
          Arg() = default;
          Arg(const Arg&) = default;
          Arg(Arg&&) = default;
          Arg& operator=(const Arg&) = delete;
          Arg& operator=(Arg&&) = default;
        };
        
        bool operator==(const Arg&, const Arg&);
        
        namespace std {
          template <>
          struct hash<Arg> {
            size_t operator()(const Arg&);
          };
        }

        fruit::Component<X> getParentComponent(int, std::string, Arg);

        fruit::Component<X> getComponent() {
          return fruit::createComponent()
            .install(getParentComponent, 5, std::string("Hello"), 15);
        }
        '''
    expect_generic_compile_error(
        'error: use of deleted function .Arg& Arg::operator=\(const Arg&\).'
            + '|error: overload resolution selected deleted operator .=.'
            + '|error C2280: .Arg &Arg::operator =\(const Arg &\).: attempting to reference a deleted function',
        COMMON_DEFINITIONS,
        source)

def test_install_with_args_error_not_equality_comparable():
    source = '''
        struct X {
          int n;
          X(int n) : n(n) {}
        };
        
        struct Arg {
          Arg() = default;
          Arg(const Arg&) = default;
          Arg(Arg&&) = default;
          Arg& operator=(const Arg&) = default;
          Arg& operator=(Arg&&) = default;
        };
        
        namespace std {
          template <>
          struct hash<Arg> {
            size_t operator()(const Arg&);
          };
        }

        fruit::Component<X> getParentComponent(int, std::string, Arg);

        fruit::Component<X> getComponent() {
          return fruit::createComponent()
            .install(getParentComponent, 5, std::string("Hello"), Arg{});
        }
        '''
    expect_generic_compile_error(
        'error: no match for .operator==. \(operand types are .const Arg. and .const Arg.\)'
            + '|error: invalid operands to binary expression \(.const Arg. and .const Arg.\)'
            + '|error C2676: binary .==.: .const Arg. does not define this operator',
        COMMON_DEFINITIONS,
        source)

def test_install_with_args_error_not_equality_comparable_with_conversion():
    source = '''
        struct X {
          int n;
          X(int n) : n(n) {}
        };
        
        struct Arg {
          Arg(int) {}
          Arg() = default;
          Arg(const Arg&) = default;
          Arg(Arg&&) = default;
          Arg& operator=(const Arg&) = default;
          Arg& operator=(Arg&&) = default;
        };
        
        namespace std {
          template <>
          struct hash<Arg> {
            size_t operator()(const Arg&);
          };
        }

        fruit::Component<X> getParentComponent(int, std::string, Arg);

        fruit::Component<X> getComponent() {
          return fruit::createComponent()
            .install(getParentComponent, 5, std::string("Hello"), 15);
        }
        '''
    expect_generic_compile_error(
        'error: no match for .operator==. \(operand types are .const Arg. and .const Arg.\)'
            + '|error: invalid operands to binary expression \(.const Arg. and .const Arg.\)'
            + '|error C2676: binary .==.: .const Arg. does not define this operator',
        COMMON_DEFINITIONS,
        source)

def test_install_with_args_error_not_hashable():
    source = '''
        struct Arg {
          Arg() = default;
          Arg(const Arg&) = default;
          Arg(Arg&&) = default;
          Arg& operator=(const Arg&) = default;
          Arg& operator=(Arg&&) = default;
        };
        
        bool operator==(const Arg&, const Arg&);
        
        fruit::Component<X> getParentComponent(int, std::string, Arg);

        fruit::Component<X> getComponent() {
          return fruit::createComponent()
            .install(getParentComponent, 5, std::string("Hello"), Arg{});
        }
        '''
    expect_generic_compile_error(
        'error: use of deleted function .std::hash<Arg>::hash\(\).'
            + '|error: call to implicitly-deleted default constructor of .std::hash<Arg>.'
            + '|error: invalid use of incomplete type .struct std::hash<Arg>.'
            + '|error: implicit instantiation of undefined template .std::(__1::)?hash<Arg>.'
            + '|error C2338: The C\+\+ Standard doesn.t provide a hash for this type.',
        COMMON_DEFINITIONS,
        source)

def test_install_with_args_error_not_hashable_with_conversion():
    source = '''
        struct Arg {
          Arg(int) {}
          Arg() = default;
          Arg(const Arg&) = default;
          Arg(Arg&&) = default;
          Arg& operator=(const Arg&) = default;
          Arg& operator=(Arg&&) = default;
        };
        
        bool operator==(const Arg&, const Arg&);
        
        fruit::Component<X> getParentComponent(int, std::string, Arg);

        fruit::Component<X> getComponent() {
          return fruit::createComponent()
            .install(getParentComponent, 5, std::string("Hello"), 15);
        }
        '''
    expect_generic_compile_error(
        'error: use of deleted function .std::hash<Arg>::hash\(\).'
            + '|error: call to implicitly-deleted default constructor of .std::hash<Arg>.'
            + '|error: invalid use of incomplete type .struct std::hash<Arg>.'
            + '|error: implicit instantiation of undefined template .std::(__1::)?hash<Arg>.'
            + '|error C2338: The C\+\+ Standard doesn.t provide a hash for this type.',
        COMMON_DEFINITIONS,
        source)

@pytest.mark.parametrize('XAnnot', [
    'X',
    'fruit::Annotated<Annotation1, X>',
])
def test_install_component_functions_deduped(XAnnot):
    source = '''
        struct X {};

        X x;

        fruit::Component<> getComponent(int) {
          return fruit::createComponent()
            .addInstanceMultibinding<XAnnot, X>(x);
        }

        fruit::Component<> getComponent2() {
          return fruit::createComponent()
            .install(getComponent, 1);
        }

        fruit::Component<> getComponent3() {
          return fruit::createComponent()
            .install(getComponent, 1);
        }

        fruit::Component<> getComponent4() {
          return fruit::createComponent()
            .install(getComponent2)
            .install(getComponent3);
        }

        int main() {
          fruit::Injector<> injector(getComponent4);

          // We test multibindings because the effect on other bindings is not user-visible (that only affects
          // performance).
          std::vector<X*> multibindings = injector.getMultibindings<XAnnot>();
          Assert(multibindings.size() == 1);
          Assert(multibindings[0] == &x);
        }
        '''
    expect_success(
        COMMON_DEFINITIONS,
        source,
        locals())

@pytest.mark.parametrize('XAnnot', [
    'X',
    'fruit::Annotated<Annotation1, X>',
])
def test_install_component_functions_different_args_not_deduped(XAnnot):
    source = '''
        struct X {};

        X x;

        fruit::Component<> getComponent(int) {
          return fruit::createComponent()
            .addInstanceMultibinding<XAnnot, X>(x);
        }

        fruit::Component<> getComponent2() {
          return fruit::createComponent()
            .install(getComponent, 1);
        }

        fruit::Component<> getComponent3() {
          return fruit::createComponent()
            .install(getComponent, 2);
        }

        fruit::Component<> getComponent4() {
          return fruit::createComponent()
            .install(getComponent2)
            .install(getComponent3);
        }

        int main() {
          fruit::Injector<> injector(getComponent4);

          // We test multibindings because the effect on other bindings is not user-visible (it only affects
          // performance).
          std::vector<X*> multibindings = injector.getMultibindings<XAnnot>();
          Assert(multibindings.size() == 2);
          Assert(multibindings[0] == &x);
          Assert(multibindings[1] == &x);
        }
        '''
    expect_success(
        COMMON_DEFINITIONS,
        source,
        locals())

def test_install_component_functions_loop():
    source = '''
        struct X {};
        struct Y {};
        struct Z {};
        
        // X -> Y -> Z -> Y
        
        fruit::Component<X> getXComponent();
        fruit::Component<Y> getYComponent();
        fruit::Component<Z> getZComponent();

        fruit::Component<X> getXComponent() {
          return fruit::createComponent()
              .registerConstructor<X()>()
              .install(getYComponent);
        }

        fruit::Component<Y> getYComponent() {
          return fruit::createComponent()
              .registerConstructor<Y()>()
              .install(getZComponent);
        }

        fruit::Component<Z> getZComponent() {
          return fruit::createComponent()
              .registerConstructor<Z()>()
              .install(getYComponent);
        }

        int main() {
          fruit::Injector<X> injector(getXComponent);
          (void)injector;
        }
        '''
    expect_runtime_error(
        'Component installation trace \(from top-level to the most deeply-nested\):\n'
            + '(class )?fruit::Component<(struct )?X> ?\((__cdecl)?\*\)\((void)?\)\n'
            + '<-- The loop starts here\n'
            + '(class )?fruit::Component<(struct )?Y> ?\((__cdecl)?\*\)\((void)?\)\n'
            + '(class )?fruit::Component<(struct )?Z> ?\((__cdecl)?\*\)\((void)?\)\n'
            + '(class )?fruit::Component<(struct )?Y> ?\((__cdecl)?\*\)\((void)?\)\n',
        COMMON_DEFINITIONS,
        source,
        locals())

def test_install_component_functions_different_arguments_loop_not_reported():
    source = '''
        struct X {};
        struct Y {};
        struct Z {};
        
        // X -> Y(1) -> Z -> Y(2)
        
        fruit::Component<X> getXComponent();
        fruit::Component<Y> getYComponent(int);
        fruit::Component<Z> getZComponent();

        fruit::Component<X> getXComponent() {
          return fruit::createComponent()
              .registerConstructor<X()>()
              .install(getYComponent, 1);
        }

        fruit::Component<Y> getYComponent(int n) {
            if (n == 1) {
                return fruit::createComponent()
                    .registerConstructor<Y()>()
                    .install(getZComponent);
            } else {
                return fruit::createComponent()
                    .registerConstructor<Y()>();
            }
        }

        fruit::Component<Z> getZComponent() {
          return fruit::createComponent()
              .registerConstructor<Z()>()
              .install(getYComponent, 2);
        }

        int main() {
          fruit::Injector<X> injector(getXComponent);
          injector.get<X>();
        }
        '''
    expect_success(
        COMMON_DEFINITIONS,
        source,
        locals())

if __name__== '__main__':
    main(__file__)
