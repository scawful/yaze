# Message Passing

## Overview

yaze includes a message passing and notification system as part of its core library. Supports message filtering, dynamic method binding, swizzling, and reflection. This message system was inspired by Objective-C and Cocoa's message passing system. It aims to overcome some of the difficulties with handling events in ImGui.

This system is currently in development and most of the content here was generated using ChatGPT to help me organize my thoughts. I will be updating this document as I continue to develop the system.

### Key Components

- **Message**: A basic unit of communication between components. It contains a type, sender, and an optional payload.
- **IMessageListener**: An interface for objects that want to receive messages. Implement this interface to handle incoming messages.
- **MessageDispatcher**: The central hub that registers listeners, dispatches messages, and manages protocols, filters, and dynamic handlers.
- **AsyncMessageDispatcher**: An extension of `MessageDispatcher` that supports asynchronous message dispatching via a queue.
- **IMessageProtocol**: An interface for defining custom protocols that can filter or handle messages based on specific criteria.
- **MessageFilter**: A class used to filter which messages a listener should receive.
- **Swizzler**: A utility that allows you to dynamically replace (swizzle) methods on objects at runtime, enabling dynamic behavior changes.
- **Reflectable**: An interface that allows for runtime inspection and manipulation of objects' properties.
- **ObjectFactory**: A factory for creating instances of `Reflectable` objects dynamically based on type names.

### Getting Started

#### 1. **Setting Up the Message Dispatcher**

To start using the message passing system, first create an instance of `MessageDispatcher` or `AsyncMessageDispatcher` depending on whether you need synchronous or asynchronous message handling.

```cpp
yaze::app::core::MessageDispatcher dispatcher;
// or for asynchronous:
yaze::app::core::AsyncMessageDispatcher async_dispatcher;
async_dispatcher.Start();
```

The EditorManager in the main yaze app will have a MessageDispatcher instance which can be injected into the various Editor components. This will allow the components to communicate with each other without needing to know about each other.

#### 2. **Registering Listeners**

Components that need to listen for messages must implement the `IMessageListener` interface and register themselves with the dispatcher.

```cpp
class MyListener : public yaze::app::core::IMessageListener {
 public:
  void OnMessageReceived(const yaze::app::core::Message& message) override {
    // Handle the message
  }
};

MyListener listener;
dispatcher.RegisterListener("MyMessageType", &listener);
```

#### 3. **Sending Messages**

To communicate between components, create a `Message` and send it through the dispatcher.

```cpp
yaze::app::core::Message message("MyMessageType", this, some_payload);
dispatcher.SendMessage(message);
```

#### 4. **Using Protocols and Filters**

For more advanced message handling, you can define custom protocols and filters. Protocols can determine if they can handle a message, while filters can refine which messages are received by a listener.

```cpp
class MyProtocol : public yaze::app::core::IMessageProtocol {
 public:
  bool CanHandleMessage(const yaze::app::core::Message& message) const override {
    // Define criteria for handling
    return message.type == "MyMessageType";
  }
};

dispatcher.RegisterProtocol(new MyProtocol());

class MyFilter : public yaze::app::core::MessageFilter {
 public:
  bool ShouldReceiveMessage(const yaze::app::core::Message& message) const override {
    // Filter logic
    return true; // Receive all messages of this type
  }
};

dispatcher.RegisterFilteredListener("MyMessageType", &listener, std::make_unique<MyFilter>());
```

#### 5. **Dynamic Method Binding and Swizzling**

To dynamically bind methods to message types or change the behavior of methods at runtime, use the `Swizzler` and `BindHandler` methods.

```cpp
dispatcher.BindHandler("MyMessageType", [](const yaze::app::core::Message& message) {
  // Handle the message dynamically
});

Swizzler swizzler;
swizzler.Swizzle(&some_object, &SomeClass::OriginalMethod, []() {
  // New method behavior
});
```

#### 6. **Reflection and Object Creation**

For systems that require dynamic inspection and manipulation of objects, implement the `Reflectable` interface and use the `ObjectFactory` to create instances dynamically.

```cpp
class MyObject : public yaze::app::core::Reflectable {
 public:
  std::string GetTypeName() const override { return "MyObject"; }
  std::vector<std::string> GetPropertyNames() const override { return {"property"}; }
  std::any GetPropertyValue(const std::string& property_name) const override {
    if (property_name == "property") return property_;
    return {};
  }
  void SetPropertyValue(const std::string& property_name, const std::any& value) override {
    if (property_name == "property") property_ = std::any_cast<int>(value);
  }

 private:
  int property_;
};

yaze::app::core::ObjectFactory factory;
factory.RegisterType<MyObject>("MyObject");
auto my_object = factory.CreateObject("MyObject");
```
