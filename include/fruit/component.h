/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FRUIT_COMPONENT_H
#define FRUIT_COMPONENT_H

// This include is not required here, but having it here shortens the include trace in error messages.
#include <fruit/impl/injection_errors.h>

#include <fruit/fruit_forward_decls.h>
#include <fruit/impl/bindings.h>
#include <fruit/impl/meta/component.h>
#include <fruit/impl/component_storage/component_storage.h>
#include <fruit/impl/component_storage/partial_component_storage.h>
#include <fruit/impl/component_functors.defn.h>

namespace fruit {

/**
 * The parameters can be of the form <P...> or <Required<R...>, P...> where:
 * * R... are the required types (types required to inject some types in P... but that are not provided by this Component), if any
 * * P... are the types provided by this Component.
 * No type can appear twice, not even once in R and once in P.
 * 
 * See PartialComponent below for the methods available in this class.
 */
template<typename... Params>
class Component {
 public:
  Component(Component &&) = default;

  Component &operator=(Component &&) = delete;
  Component &operator=(const Component &) = delete;

  /**
   * Converts a PartialComponent to an arbitrary Component, auto-injecting the missing types (if
   * any).
   * This is usually called implicitly when returning a component from a function. See
   * PartialComponent for an example.
   */
  template<typename... Bindings>
  Component(PartialComponent<Bindings...> component);

 private:
  // Do not use. Use fruit::createComponent() instead.
  Component() = default;

  template<typename... Types>
  friend class Component;

  template<typename... Bindings>
  friend class PartialComponent;

  template<typename... OtherParams>
  friend class NormalizedComponent;

  template<typename... OtherParams>
  friend class Injector;

  template <typename... Bindings>
  friend class fruit::impl::PartialComponentStorage;

  template <typename Component, typename... Args>
  friend class fruit::impl::LazyComponentImpl;

  friend struct fruit::impl::ComponentStorageEntry::LazyComponentWithNoArgs;

  template <typename Component, typename... Args>
  friend class fruit::impl::ComponentInterfaceImpl;

  fruit::impl::ComponentStorage storage;

  using Comp = fruit::impl::meta::Eval<fruit::impl::meta::ConstructComponentImpl(fruit::impl::meta::Type<Params>...)>;

  using Check1 = typename fruit::impl::meta::CheckIfError<Comp>::type;
  // Force instantiation of Check1.
  static_assert(true || sizeof(Check1), "");
};

/**
 * Constructs an empty component. Note that EmptyPartialComponent is a subclass of PartialComponent<> and has no additional methods,
 * so see PartialComponent<> for the methods that you can call on an EmptyPartialComponent.
 *
 * Example usage:
 * 
 * const fruit::Component<Foo>& getFooComponent() {
 *   static const fruit::Component<Foo> comp = fruit::createComponent()
 *      .install(getComponent1)
 *      .install(getComponent2)
 *      .bind<Foo, FooImpl>();
 *   return comp;
 * }
 * 
 * Since types are auto-injected when needed, just converting this to the desired component can suffice in some cases, e.g.:
 * 
 * const fruit::Component<Foo>& getFooComponent() {
 *   static const fruit::Component<Foo> comp = fruit::createComponent();
 *   return comp;
 * }
 * 
 * This works if Foo has an Inject typedef or a constructor wrapped in INJECT.
 *
 * WARNING: the resulting component *must* be converted to a Component (unless an exception is thrown).
 * If it isn't, Fruit will abort the program.
 * Note that the return type of createComponent() is PartialComponent, not Component, so that too needs to be converted.
 * Therefore, using auto to avoid mentioning the type of `comp' again will not work, since the type is PartialComponent and not Component.
 */
PartialComponent<> createComponent();

/**
 * A partially constructed component.
 * 
 * Client code should never write `PartialComponent'; always start the construction of a component with fruit::createComponent(),
 * and end it by casting the PartialComponent to the desired Component (often done implicitly by returning a PartialComponent
 * from a function that has Component<...> as the return type).
 * 
 * The template parameter is used to propagate information about bound types, it is purely an implementation detail; users of the
 * Fruit library can pretend that this class is not templated, in no case a specific template parameter is required. All methods
 * of this class take `this' as &&, allowing to chain method invocations without declaring a variable of type PartialComponent.
 * 
 * Example usage:
 * 
 * fruit::Component<Foo> getFooComponent() {
 *   return fruit::createComponent()
 *      .install(getComponent1)
 *      .install(getComponent2)
 *      .bind<Foo, FooImpl>();
 * }
 * 
 * Note that no variable of type PartialComponent has been declared; this class should only be used for temporary values.
 */
template<typename... Bindings>
class PartialComponent {
public:
  PartialComponent(PartialComponent &&) = default;

  PartialComponent &operator=(PartialComponent &&) = delete;
  PartialComponent &operator=(const PartialComponent &) = delete;

  /**
   * Binds the base class (typically, an interface or abstract class) I to the implementation C.
   * 
   * This supports annotated injection, just wrap I and/or C in fruit::Annotated<> if desired.
   */
  template<typename I, typename C>
  PartialComponent<fruit::impl::Bind<I, C>, Bindings...> bind();

  /**
   * Registers Signature as the constructor signature to use to inject a type.
   * 
   * Example usage:
   * 
   * fruit::createComponent()
   *     .registerConstructor<Foo(Bar*,Baz*)>() // Registers the constructor Foo::Foo(Bar*,Baz*)
   * 
   * It's usually more convenient to use an INJECT macro or Inject typedef instead, e.g.:
   * 
   * class Foo {
   * public:
   *   // This also declares the constructor
   *   INJECT(Foo(Bar* bar, Baz* baz));
   * ...
   * };
   * 
   * or (equivalent):
   * 
   * class Foo {
   * public:
   *   using Inject = Foo(Bar*, Baz*);
   *   Foo(Bar* bar, Baz* baz);
   * ...
   * };
   * 
   * Use registerConstructor() when you want to inject the class C in different ways in different components (just make sure those
   * don't end up in the same injector), or when C is a third-party class that can't be modified.
   * 
   * This supports annotated injection, just wrap the desired types (return type and/or argument types of the signature)
   * with fruit::Annotated<> if desired.
   */
  template<typename Signature>
  PartialComponent<fruit::impl::RegisterConstructor<Signature>, Bindings...> registerConstructor();

  /**
   * Use this method to bind the type C to a specific instance.
   * The caller must ensure that the provided reference is valid for the entire lifetime of the component and of any components
   * or injectors that install this component; the caller must also ensure that the object is destroyed after the last
   * components/injectors using it are destroyed.
   * 
   * Example usage:
   * 
   * NormalizedComponent<...> normalizedComponent = ...;
   * Request request;
   * Injector<...> injector(normalizedComponent,
   *                        Component<Request>(fruit::createComponent()
   *                            .bindInstance(request)));
   * 
   * This should be used sparingly (let Fruit handle the object lifetime when possible), but in some cases it is necessary; for
   * example, if a web server creates an injector to handle each request, this method can be used to inject the request itself.
   */
  template<typename C>
  PartialComponent<fruit::impl::BindInstance<C, C>, Bindings...> bindInstance(C& instance);

  /**
   * Similar to the previous, but binds a const&. Note that the reference must still outlive the component/injector
   * as in the non-const case.
   */
  template<typename C>
  PartialComponent<fruit::impl::BindConstInstance<C, C>, Bindings...> bindInstance(const C& instance);

  /**
   * This is deleted to catch cases where the instance would likely be destroyed before the component/injectors.
   */
  template<typename C>
  PartialComponent<fruit::impl::BindConstInstance<C, C>, Bindings...> bindInstance(C&&) = delete;

  /**
   * Similar to the first version of bindInstance(), but allows to specify an annotated type that
   * will be bound to the specified value.
   * For example, to bind an instance to the type Annotated<Hostname, std::string>, you can use:
   * 
   * fruit::createComponent()
   *     .bindInstance<fruit::Annotated<Hostname, std::string>>(hostname)
   */
  template<typename AnnotatedType, typename C>
  PartialComponent<fruit::impl::BindInstance<AnnotatedType, C>, Bindings...> bindInstance(C& instance);

  /**
   * Similar to the previous, but binds a const&. Note that the reference must still outlive the component/injector
   * as in the non-const case.
   */
  template<typename AnnotatedType, typename C>
  PartialComponent<fruit::impl::BindConstInstance<AnnotatedType, C>, Bindings...> bindInstance(const C& instance);

  /**
   * This is deleted to catch cases where the instance would likely be destroyed before the component/injectors.
   */
  template<typename AnnotatedType, typename C>
  PartialComponent<fruit::impl::BindConstInstance<AnnotatedType, C>, Bindings...> bindInstance(C&& instance);

  /**
   * Registers `provider' as a provider of C, where provider is a lambda with no captures returning either C or C* (prefer
   * returning a C by value instead of allocating a C using `new C', to avoid the allocation).
   * 
   * When injecting a C, the arguments of the provider will be injected and the provider will then be called to create the
   * C instance, that will then be stored in the injector.
   * 
   * If `provider' returns a pointer, it must be non-null; otherwise the program will abort.
   * 
   * Example:
   * 
   * registerProvider([](Bar* bar, Baz* baz) {
   *    Foo foo(bar, baz);
   *    foo.initialize();
   *    return std::move(foo);
   * })
   * 
   * As in the previous example, it's not necessary to specify the type parameter, it will be inferred by the compiler.
   * 
   * registerProvider() can't be called with a plain function, but you can write a lambda that wraps the function to achieve the
   * same result.
   * 
   * Registering a stateful functors (including lambdas with captures) is NOT supported.
   * However, you can write something like: 
   * 
   * struct Functor {
   *   Functor(int n) {...}
   *   MyClass operator()(Foo* foo) {...}
   * };
   * 
   * Component<MyClass> getMyClassComponent() {
   *   static const Functor aFunctor(42);
   *   return fruit::createComponent()
   *       ... // Bind Foo
   *       .bindInstance(aFunctor)
   *       .registerProvider([](Functor functor, Foo* foo) { return functor(foo); });
   * }
   */
  template<typename Lambda>
  PartialComponent<fruit::impl::RegisterProvider<Lambda>, Bindings...> registerProvider(Lambda lambda);

  /**
   * Similar to the previous version of registerProvider(), but allows to specify an annotated type
   * for the provider. This allows to inject annotated types in the parameters and/or bind the
   * provider to an annotated type. For example:
   * 
   * .registerProvider<Annotated<MyAnnotation, Foo>(Annotated<SomeOtherAnnotation, Bar*>, Baz*)>(
   *    [](Bar* bar, Baz* baz) {
   *      Foo foo(bar, baz);
   *      foo.initialize();
   *      return std::move(foo);
   *    })
   * 
   * Binds the type Foo (annotated with MyAnnotation) and injects the Bar annotated with
   * SomeOtherAnnotation as the first parameter of the lambda.
   */
  template<typename AnnotatedSignature, typename Lambda>
  PartialComponent<fruit::impl::RegisterProvider<AnnotatedSignature, Lambda>, Bindings...> registerProvider(Lambda lambda);

  /**
   * Similar to bind<I, C>(), but adds a multibinding instead.
   * 
   * Multibindings are independent from bindings; creating a binding with bind doesn't count as a multibinding, and adding a
   * multibinding doesn't allow to inject the type (it only allows to retrieve multibindings through the getMultibindings method
   * of the injector).
   * 
   * Unlike bindings, where adding a the same binding twice is allowed (and ignored), adding the same multibinding multiple times
   * will result in the creation of multiple "equivalent" instances, that will all be returned by getMultibindings().
   * It is good practice to add the multibindings in a component that is "close" to the injector, to avoid installing that
   * component more than once.
   * 
   * This supports annotated injection, just wrap I and/or C in fruit::Annotated<> if desired.
   */
  template<typename I, typename C>
  PartialComponent<fruit::impl::AddMultibinding<I, C>, Bindings...> addMultibinding();

  /**
   * Similar to bindInstance(), but adds a multibinding instead.
   * 
   * Multibindings are independent from bindings; creating a binding with bindInstance doesn't count as a
   * multibinding, and adding a multibinding doesn't allow to inject the type (only allows to retrieve
   * multibindings through the getMultibindings method of the injector).
   * 
   * Unlike bindings, where adding a the same binding twice is allowed (and ignored), adding several multibindings for the same
   * instance will result in duplicated values in the result of getMultibindings.
   * It is good practice to add the multibindings in a component that is "close" to the injector, to avoid installing that
   * component more than once.
   * 
   * This method adds a multibinding for C. If the object implements an interface I and you want to add a multibinding
   * for that interface instead, cast the object to I& before calling this.
   * 
   * Note that this takes the instance by reference, not by value; it must remain valid for the entire lifetime of this component
   * and of any injectors created from this component.
   */
  template<typename C>
  PartialComponent<fruit::impl::AddInstanceMultibinding<C>, Bindings...> addInstanceMultibinding(C &instance);

  /**
   * Similar to the previous version of addInstanceMultibinding(), but allows to specify an
   * annotated type.
   * Example use:
   * 
   * createComponent()
   *     // With someObject of type MyClass
   *     .addInstanceMultibinding<Annotated<MyAnnotation, MyClass>>(someObject)
   */
  template<typename AnnotatedC, typename C>
  PartialComponent<fruit::impl::AddInstanceMultibinding<AnnotatedC>, Bindings...> addInstanceMultibinding(C &instance);

  /**
   * Equivalent to calling addInstanceMultibinding() for each elements of `instances'.
   * See the documentation of addInstanceMultibinding() for more details.
   * 
   * Note that this takes the vector by reference, not by value; the vector (and its elements) must remain valid for the entire
   * lifetime of this component and of any injectors created from this component.
   */
  template<typename C>
  PartialComponent<fruit::impl::AddInstanceVectorMultibindings<C>, Bindings...> addInstanceMultibindings(std::vector<C> &instances);

  /**
   * Similar to the previous version of addInstanceMultibindings(), but allows to specify an
   * annotated type.
   * Example use:
   * 
   * createComponent()
   *     // With v of type std::vector<MyClass>
   *     .addInstanceMultibindings<Annotated<MyAnnotation, MyClass>>(v)
   */
  template<typename AnnotatedC, typename C>
  PartialComponent<fruit::impl::AddInstanceVectorMultibindings<AnnotatedC>, Bindings...> addInstanceMultibindings(std::vector<C> &instance);

  /**
   * Similar to registerProvider, but adds a multibinding instead.
   * 
   * Multibindings are independent from bindings; creating a binding with registerProvider doesn't count as a
   * multibinding, and adding a multibinding doesn't allow to inject the type (only allows to retrieve multibindings
   * through the getMultibindings method of the injector).
   * 
   * Unlike bindings, where adding a the same binding twice is allowed (and ignored), adding the same multibinding provider
   * multiple times will result in the creation of multiple "equivalent" instances, that will all be returned by getMultibindings.
   * It is good practice to add the multibindings in a component that is "close" to the injector in the get*Component call chain,
   * to avoid adding the same multibinding more than once.
   * 
   * Note that this method adds a multibinding for the type returned by the provider. If the returned object implements an
   * interface I and you want to add a multibinding for that interface instead, return a pointer casted to I*.
   */
  template<typename Lambda>
  PartialComponent<fruit::impl::AddMultibindingProvider<Lambda>, Bindings...> addMultibindingProvider(Lambda lambda);

  /**
   * Similar to the previous version of addMultibindingProvider(), but allows to specify an annotated type
   * for the provider. This allows to inject annotated types in the parameters and/or bind the
   * provider to an annotated type. For example:
   * 
   * .addMultibindingProvider<Annotated<MyAnnotation, Foo>(Annotated<SomeOtherAnnotation, Bar*>, Baz*)>(
   *    [](Bar* bar, Baz* baz) {
   *      Foo foo(bar, baz);
   *      foo.initialize();
   *      return std::move(foo);
   *    })
   * 
   * Add a multibinding for the type Foo (annotated with MyAnnotation) and injects the Bar annotated with
   * SomeOtherAnnotation as the first parameter of the lambda.
   */
  template<typename AnnotatedSignature, typename Lambda>
  PartialComponent<fruit::impl::AddMultibindingProvider<AnnotatedSignature, Lambda>, Bindings...> addMultibindingProvider(Lambda lambda);

  /**
   * Registers `factory' as a factory of C, where `factory' is a lambda with no captures returning C.
   * This is typically used for assisted injection (but can also be used if no parameters are assisted).
   * 
   * C can be any class type. If C is std::unique_ptr<T>, the factory together with a bind<I,C> in the same component
   * will automatically bind the corresponding std::function that returns a std::unique_ptr<I>.
   * 
   * C can NOT be a pointer type. If you don't want to return by value, return a std::unique_ptr instead of a naked pointer.
   * 
   * Example:
   * 
   * Component<std::function<std::unique_ptr<MyClass>(int)>> getMyClassComponent() {
   *   fruit::createComponent()
   *       ... // Bind Foo
   *       .registerFactory<std::unique_ptr<MyClass>(Foo*, Assisted<int>)>(
   *          [](Foo* foo, int n) {
   *              return std::unique_ptr<MyClass>(new MyClass(foo, n));
   *          });
   * }
   * 
   * and then, e.g. in main():
   * 
   * Injector<std::function<std::unique_ptr<MyClass>(int)>> injector(getMyClassComponent());
   * 
   * std::function<std::unique_ptr<MyClass>(int)> factory(injector);
   * std::unique_ptr<MyClass> x = factory(42);
   * 
   * Note that non-assisted parameters will be passed automatically by Fruit.
   * 
   * Unlike registerProvider(), where the signature is inferred, for this method the signature (including any Assisted
   * annotations) must be specified explicitly, while the second template parameter is inferred.
   * 
   * If the only thing that the factory does is to call new and the constructor of the class, it's usually more convenient to use
   * an Inject typedef or INJECT macro instead, e.g. the following are equivalent to the above:
   * 
   * class MyClass {
   * public:
   *    using Inject = MyClass(Foo*, Assisted<int>);
   * 
   *    MyClass(Foo* foo, int n) {...}
   * };
   * 
   * or:
   * 
   * class MyClass {
   * public:
   *    INJECT(MyClass(Foo* foo, ASSISTED(int) n)) {...}
   * };
   * 
   * Use registerFactory() when you want to inject the class in different ways in different components (just make sure those
   * don't end up in the same injector), or when MyClass is a third-party class that can't be modified.
   * 
   * registerFactory() can't be called with a plain function, but you can write a lambda that wraps the function to achieve the
   * same result.
   * 
   * Registering stateful functors (including lambdas with captures) is NOT supported.
   * However, you can write something like: 
   * 
   * struct Functor {
   *   Functor(float x) {...}
   *   std::unique_ptr<MyClass> operator()(Foo* foo, int n) {...}
   * };
   * 
   * Component<std::function<std::unique_ptr<MyClass>(int)>> getMyClassComponent() {
   *   static const Functor aFunctor(42.0);
   *   return fruit::createComponent()
   *       ... // Bind Foo
   *       .bindInstance(aFunctor)
   *       .registerFactory<std::unique_ptr<MyClass>(Functor functor, Foo*, Assisted<int>)>(
   *            [](Functor* functor, Foo* foo, int n) { return functor(foo, n); });
   * }
   */
  template<typename DecoratedSignature, typename Factory>
  PartialComponent<fruit::impl::RegisterFactory<DecoratedSignature, Factory>, Bindings...> registerFactory(Factory factory);

  /**
   * Adds the bindings (and multibindings) in the Component obtained by calling fun(args...) to the current component.
   *
   * For example, these components:
   * Component<Foo> getComponent1();
   * Component<Bar> getComponent2(int n, std::string s);
   *
   * can be installed as:
   *
   * createComponent()
   *    .install(getComponent1)
   *    .install(getComponent2, 5, std::string("Hello"))
   *
   * If any `args` are provided, they must be:
   * - Copy-constructible
   * - Move-constructible
   * - Assignable
   * - Move-assignable
   * - Equality comparable (i.e., operator== must be defined for two values of that type)
   * - Hashable (i.e., std::hash must be defined for values of that type)
   *
   * Note that this only applies to `args`. E.g. in the example above `int` and `std::string` must satisfy this
   * requirement (and they do), but `Foo` and `Bar` don't need to.
   *
   * A lambda with no captures can also be used as the first argument, for example:
   *
   * createComponent()
   *     .install([]() {return getComponent1();})
   *     .install([](int n, std::string s) {return getComponent2(n, s);}, 5, std::string("Hello"))
   *
   * These two install() calls are equivalent to the previous ones.
   *
   * As in the example, the template parameters for this method will be inferred by the compiler, it's not necessary to
   * specify them explicitly.
   */
  template <typename... OtherComponentParams, typename... FormalArgs, typename... Args>
  PartialComponent<fruit::impl::InstallComponent<fruit::Component<OtherComponentParams...>(FormalArgs...)>, Bindings...> install(
      fruit::Component<OtherComponentParams...>(*)(FormalArgs...),
      Args&&... args);

  /**
   * This class is returned by PartialComponent::replace, see the documentation of that method for more information.
   */
  template <typename ReplacedComponent, typename... GetReplacedComponentFormalArgs>
  class PartialComponentWithReplacementInProgress {
  private:
    using storage_t = fruit::impl::PartialComponentStorage<fruit::impl::PartialReplaceComponent<ReplacedComponent(GetReplacedComponentFormalArgs...)>, Bindings...>;

  public:
    template <typename... FormalArgs, typename... Args>
    PartialComponent<fruit::impl::ReplaceComponent<ReplacedComponent(GetReplacedComponentFormalArgs...), ReplacedComponent(FormalArgs...)>, Bindings...>
        with(
            ReplacedComponent(*)(FormalArgs...),
            Args&&... args);

    PartialComponentWithReplacementInProgress(storage_t storage)
      : storage(storage) {
    }

  private:
    storage_t storage;

    PartialComponentWithReplacementInProgress() = delete;
  };

  /**
   * This allows to replace an installed Component with another one. This is useful for testing.
   * For example, if you have these components:
   *
   * fruit::Component<MyDependency> getDependencyComponent() {...}
   *
   * fruit::Component<Foo> getFooComponent() {
   *     return fruit::createComponent()
   *         .install(getDependencyComponent)
   *         .bind<Foo, FooImpl>();
   * }
   *
   * fruit::Component<Bar> getBarComponent() {
   *     return fruit::createComponent()
   *         .install(getFooComponent)
   *         .bind<Bar, BarImpl>();
   * }
   *
   * When you test Bar, you might want to replace getDependencyComponent with a component that binds a fake
   * MyDependency:
   *
   * fruit::Component<MyDependency> getFakeDependencyComponent() {...}
   *
   * To do so, you can define a component like:
   *
   * fruit::Component<Bar> getBarComponentWithFakeDependency() {
   *     return fruit::createComponent()
   *         .replace(getDependencyComponent).with(getFakeDependencyComponent)
   *         .install(getBarComponent);
   * }
   *
   * This component is equivalent to:
   *
   * fruit::Component<Bar> getBarComponentWithFakeDependency() {
   *     return fruit::createComponent()
   *         .install(getFakeDependencyComponent)
   *         .bind<Foo, FooImpl>()
   *         .bind<Bar, BarImpl>();
   * }
   *
   * However this way you don't need to duplicate the bindings for Foo and Bar, and you don't even need to include them
   * in the translation unit (i.e., cc/cpp file) that defines getBarComponentWithFakeDependency().
   * In codebases with many layers, this can save a lot of duplication.
   *
   * Note that the .replace(...).with(...) must appear *before* installing the component to which it's applied to
   * (e.g., in the example above note how we install getBarComponent after the replacement in
   * getBarComponentWithFakeDependency).
   * If you add a replacement after the replaced component has been installed, Fruit will report an error at run-time.
   *
   * In the example above, the replaced and replacement component functions had no args, however it's also possible to
   * replace component function with args. The args of the replaced and replacement component functions are independent;
   * for example .replace(getDependencyComponentWithArgs, 15).with(myFakeComponentWithNoArgs) is allowed and it would
   * replace all install(getDependencyComponentWithArgs, 15) calls with install(myFakeComponentWithNoArgs).
   *
   * The component types returned by the replaced and replacement components must be the same. For example, this is NOT
   * allowed:
   *
   * fruit::Component<MyDependency, SomethingElse> getFakeDependencyComponentWithSomethingElse() {...}
   *
   * fruit::Component<Bar> getBarComponentWithFakeDependency() {
   *     return fruit::createComponent()
   *         .replace(getDependencyComponent).with(getFakeDependencyComponentWithSomethingElse) // error!
   *         .install(getBarComponent);
   * }
   *
   * Replacing a replaced component is also not allowed (and this will be reported at runtime):
   *
   * fruit::Component<MyDependency> getOtherFakeDependencyComponent() {...}
   *
   * fruit::Component<Bar> getBarComponentWithOtherFakeDependency() {
   *     return fruit::createComponent()
   *         .replace(getFakeDependencyComponent).with(getOtherFakeDependencyComponent) // you can't do this!
   *         .install(getBarComponent);
   * }
   *
   * However note that you *can* replace components that define replacements, for example:
   *
   * fruit::Component<> getFakeDependencyReplacementComponent() {
   *     return fruit::createComponent()
   *         .replace(getDependencyComponent).with(getFakeDependencyComponentWithSomethingElse);
   * }
   *
   * fruit::Component<...> getComponent() {
   *     return fruit::createComponent()
   *         .replace(getFakeDependencyReplacementComponent).with(...) // Ok
   *         .install(...);
   * }
   *
   * Replacements are only installed if the replaced component is installed, otherwise they are ignored.
   * In the example above, if getFooComponent didn't install getDependencyComponent, when a test creates an
   * injector for getBarComponentWithFakeDependency it would not install getFakeDependencyComponent.
   *
   * Unlike bindings, when creating an injector from a NormalizedComponent and a Component the replacements in the
   * NormalizedComponent do *not* affect the install()s in the Component and vice versa.
   * If you want a replacement to apply to both, you should add it in both.
   *
   * Replacements can also be chained, for example:
   *
   * fruit::Component<...> getRootComponent() {
   *     return fruit::createComponent()
   *         .replace(getComponent1).with(getComponent2)
   *         .replace(getComponent2).with(getComponent3)
   *         .install(getComponent1);
   * }
   *
   * is equivalent to:
   *
   * fruit::Component<...> getRootComponent() {
   *     return fruit::createComponent()
   *         .install(getComponent3);
   * }
   *
   * In this case, the order in which you add the replacements in the chain does not matter, as long as all the
   * replacements in the chain have been added before the install().
   * Of course this is a simple example, in the real world the replacements and the install would probably come from
   * other components.
   */
  template <typename... OtherComponentParams, typename... FormalArgs, typename... Args>
  PartialComponentWithReplacementInProgress<fruit::Component<OtherComponentParams...>, FormalArgs...> replace(
      fruit::Component<OtherComponentParams...>(*)(FormalArgs...),
      Args&&... args);

  ~PartialComponent();

private:
  template<typename... OtherBindings>
  friend class PartialComponent;

  template<typename... Types>
  friend class Component;

  fruit::impl::PartialComponentStorage<Bindings...> storage;

  // Do not use. Use fruit::createComponent() instead.
  PartialComponent() = delete;

  // Do not use. Only use PartialComponent for temporaries, and then convert it to a Component.
  PartialComponent(const PartialComponent &) = delete;

  PartialComponent(fruit::impl::PartialComponentStorage<Bindings...> storage);

  template<typename NewBinding>
  using OpFor = typename fruit::impl::meta::OpForComponent<Bindings...>::template AddBinding<NewBinding>;

  friend PartialComponent<> createComponent();
};

} // namespace fruit

#include <fruit/impl/component.defn.h>


#endif // FRUIT_COMPONENT_H
