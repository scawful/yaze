# Message Passing

## Overview

yaze includes a message passing and notification system as part of its core library. This system allows different parts of your application to communicate through messages, enabling a loosely coupled architecture that enhances modularity, maintainability, and scalability. The system supports advanced features such as message filtering, dynamic method binding, swizzling, and reflection, making it highly dynamic and adaptable for complex applications, including those requiring plugin or extension systems.

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
- **Notification**: A specialized message used to broadcast events to multiple observers.
- **NotificationCenter**: A central hub for managing notifications and observers.

### Getting Started

#### 1. **Setting Up the Message Dispatcher**

To start using the message passing system, first create an instance of `MessageDispatcher` or `AsyncMessageDispatcher` depending on whether you need synchronous or asynchronous message handling.

```cpp
yaze::app::core::MessageDispatcher dispatcher;
// or for asynchronous:
yaze::app::core::AsyncMessageDispatcher async_dispatcher;
async_dispatcher.Start();
```

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


### Using the Notification System

The yaze notification system that allows for broadcasting events to multiple observers. This is particularly useful when you need to inform several parts of the editor about a state change or event without tightly coupling them.

#### 1. **Setting Up the Notification Center**

To use notifications, create an instance of `NotificationCenter`.

```cpp
yaze::app::core::NotificationCenter notification_center;
```

#### 2. **Adding and Removing Observers**

Observers, which are typically components implementing the `IMessageListener` interface, can register themselves to listen for specific notifications.

```cpp
MyListener observer;
notification_center.AddObserver("MyNotificationType", &observer);
```

If an observer no longer needs to listen for notifications, it can be removed:

```cpp
notification_center.RemoveObserver("MyNotificationType", &observer);
```

#### 3. **Posting Notifications**

To broadcast a notification, create a `Notification` object and post it through the `NotificationCenter`. All registered observers will receive the notification.

```cpp
yaze::app::core::Notification notification("MyNotificationType", this, some_payload);
notification_center.PostNotification(notification);
```

### Extending the System

This system is designed to be highly extensible. You can create plugins that register their own message handlers, protocols, and dynamic behaviors. Additionally, the use of method swizzling and reflection allows for runtime changes to object behavior and properties, making this system ideal for applications that require a high degree of flexibility and modularity.
