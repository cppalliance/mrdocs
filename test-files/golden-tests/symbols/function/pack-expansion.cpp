// Test parameter pack expansion in function parameters
// Issue #1129: Args&&... args rendered as Args...&&... args

template<class T, class... Args>
T* make_ptr(Args&&... args);

template<class... Args>
void forward_all(Args&&... args);

template<typename... Ts>
void by_value(Ts... args);

template<typename... Ts>
void by_ref(Ts&... args);

template<typename... Ts>
void by_const_ref(const Ts&... args);

template<typename... Ts>
void by_pointer(Ts*... args);
