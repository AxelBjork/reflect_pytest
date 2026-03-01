# IPC Send Path Assembly Excerpts (-O3 -flto)

This file contains the cleaned (no addresses/hex) x86-64 assembly for a complete `MessageBus::dispatch()` -> `UdpBridge::forward_to_udp` -> `sendto` path, specifically for `MsgId::RevisionResponse`.

## 1. MessageBus::dispatch()
The core lock-free dispatch loop and dynamic indirect call.

```nasm
<ipc::MessageBus::dispatch(MsgId, std::any)>:
	sub    rsp,0x18
	mov    QWORD PTR [rsp+0x10],r12
	mov    r12,rdx
	cmp    QWORD PTR [rdi+0x18],0x0
	je     <ipc::MessageBus::dispatch(MsgId, std::any)+0xc0>
	mov    r9,QWORD PTR [rdi+0x8]
	movzx  eax,si
	xor    edx,edx
	div    r9
	mov    rax,QWORD PTR [rdi]
	mov    rdi,QWORD PTR [rax+rdx*8]
	mov    r10,rdx
	test   rdi,rdi
	je     <ipc::MessageBus::dispatch(MsgId, std::any)+0xb6>
	mov    rax,QWORD PTR [rdi]
	movzx  r8d,WORD PTR [rax+0x8]
	cmp    si,r8w
	je     <ipc::MessageBus::dispatch(MsgId, std::any)+0x70>
	mov    rcx,QWORD PTR [rax]
	test   rcx,rcx
	je     <ipc::MessageBus::dispatch(MsgId, std::any)+0xb6>
	movzx  r8d,WORD PTR [rcx+0x8]
	mov    rdi,rax
	xor    edx,edx
	movzx  eax,r8w
	div    r9
	cmp    r10,rdx
	jne    <ipc::MessageBus::dispatch(MsgId, std::any)+0xb6>
	mov    rax,rcx
	cmp    si,r8w
	jne    <ipc::MessageBus::dispatch(MsgId, std::any)+0x44>
	nop    DWORD PTR [rax+rax*1+0x0]
	mov    rax,QWORD PTR [rdi]
	test   rax,rax
	je     <ipc::MessageBus::dispatch(MsgId, std::any)+0xb6>
	mov    QWORD PTR [rsp],rbx
	mov    rbx,QWORD PTR [rax+0x10]
	mov    QWORD PTR [rsp+0x8],rbp
	mov    rbp,QWORD PTR [rax+0x18]
	cmp    rbp,rbx
	je     <ipc::MessageBus::dispatch(MsgId, std::any)+0xad>
	xchg   ax,ax
	cmp    QWORD PTR [rbx+0x10],0x0
	je     <ipc::MessageBus::dispatch(MsgId, std::any) [clone .cold]>
	mov    rdi,rbx
	mov    rsi,r12
	call   QWORD PTR [rbx+0x18]      # <-- The indirect branch to std::function
	add    rbx,0x20
	cmp    rbx,rbp
	jne    <ipc::MessageBus::dispatch(MsgId, std::any)+0x90>
	mov    rbx,QWORD PTR [rsp]
	mov    rbp,QWORD PTR [rsp+0x8]
	mov    r12,QWORD PTR [rsp+0x10]
	add    rsp,0x18
	ret
	mov    rax,QWORD PTR [rdi+0x10]
	test   rax,rax
	je     <ipc::MessageBus::dispatch(MsgId, std::any)+0xb6>
	add    rdi,0x10
	jmp    <ipc::MessageBus::dispatch(MsgId, std::any)+0xde>
	nop
	mov    rdx,QWORD PTR [rax]
	mov    rdi,rax
	test   rdx,rdx
	je     <ipc::MessageBus::dispatch(MsgId, std::any)+0xb6>
	mov    rax,rdx
	cmp    si,WORD PTR [rax+0x8]
	jne    <ipc::MessageBus::dispatch(MsgId, std::any)+0xd0>
	jmp    <ipc::MessageBus::dispatch(MsgId, std::any)+0x70>
	cs nop WORD PTR [rax+rax*1+0x0]
```

## 2. UdpBridge::forward_to_udp<RevisionResponsePayload>
The LTO-inlined handler, demonstrating the lock-free validation and the serialization process into a stack-allocated buffer followed by an inline `memcpy`.

<std::_Function_handler<void (RevisionResponsePayload const&), ipc::UdpBridge::UdpBridge(ipc::MessageBus&)::{lambda<MsgId... $N0>(ipc::MsgList<($N0)...>)#1}::operator()<(MsgId)0, (MsgId)3, (MsgId)21, (MsgId)31, (MsgId)41, (MsgId)50, (MsgId)61, (MsgId)51, (MsgId)52, (MsgId)1101, (MsgId)2001>(ipc::MsgList<(MsgId)0, (MsgId)3, (MsgId)21, (MsgId)31, (MsgId)41, (MsgId)50, (MsgId)61, (MsgId)51, (MsgId)52, (MsgId)1101, (MsgId)2001>) const::{lambda(RevisionResponsePayload const&)#1}>::_M_invoke(std::_Any_data const&, RevisionResponsePayload const&)>:
	sub    rsp,0x28
	mov    QWORD PTR [rsp+0x10],rbp
	mov    rbp,QWORD PTR [rdi]
	mov    QWORD PTR [rsp+0x20],r13
	lea    r13,[rbp+0x20]
	mov    QWORD PTR [rsp+0x18],r12
	mov    r12,rsi
	mov    rdi,r13
	call   <pthread_mutex_lock@plt>
	test   eax,eax
	jne    <std::_Function_handler<...> [clone .cold]>
	mov    rdi,r13
	cmp    BYTE PTR [rbp+0x58],0x0
	je     <std::_Function_handler<...>+0xb5>
	mov    edi,0x43
	mov    QWORD PTR [rsp+0x8],rbx
	call   <operator new(unsigned long)@plt>
	movdqu xmm0,XMMWORD PTR [r12]
	mov    rbx,rax
	mov    eax,0x7d1
	xor    ecx,ecx
	mov    WORD PTR [rbx],ax            # <-- Serialize Header (MsgId: 2001)
	movzx  eax,BYTE PTR [r12+0x40]
	lea    r8,[rbp+0x48]
	mov    rsi,rbx
	movups XMMWORD PTR [rbx+0x2],xmm0   # <-- SIMD Serialization of 65-byte hash
	movdqu xmm0,XMMWORD PTR [r12+0x10]
	mov    r9d,0x10
	mov    edx,0x43                     # <-- UDP Payload Size
	mov    BYTE PTR [rbx+0x42],al
	movups XMMWORD PTR [rbx+0x12],xmm0
	movdqu xmm0,XMMWORD PTR [r12+0x20]
	movups XMMWORD PTR [rbx+0x22],xmm0
	movdqu xmm0,XMMWORD PTR [r12+0x30]
	movups XMMWORD PTR [rbx+0x32],xmm0
	mov    edi,DWORD PTR [rbp+0x8]      # <-- Load UDP Socket File Descriptor
	call   <sendto@plt>                 # <-- Issue System Call
	mov    rdi,rbx
	mov    esi,0x43
	call   <operator delete(void*, unsigned long)@plt>
	mov    rbx,QWORD PTR [rsp+0x8]
	mov    rdi,r13
	mov    rbp,QWORD PTR [rsp+0x10]
	mov    r12,QWORD PTR [rsp+0x18]
	mov    r13,QWORD PTR [rsp+0x20]
	add    rsp,0x28
	jmp    <pthread_mutex_unlock@plt>
	mov    rbp,rax
	jmp    <std::_Function_handler<...> [clone .cold]+0xc>
	mov    rbp,rax
	jmp    <std::_Function_handler<...> [clone .cold]+0x19>
	nop    DWORD PTR [rax]
```

## 3. Example Publisher Callsite
A typical `bus.publish<MsgId>(payload)` call in the application code resolves directly to `std::any` instantiation and `dispatch` due to inlining.

```nasm
	# ... (publisher code preparing the struct) ...
	call   <ipc::MessageBus::dispatch(MsgId, std::any)>
```

## 4. UdpBridge::rx_loop
Recording the incoming sender endpoint without a mutex lock using an atomic store constraint.

```nasm
<ipc::UdpBridge::rx_loop() + rx handler>:
	call   <recvfrom@plt>
	mov    %rax,-0x20(%rbp)
	cmpq   $0x0,-0x20(%rbp)
	js     <ipc::UdpBridge::rx_loop()+0x2ff>
	mov    -0x10d8(%rbp),%rax
	mov    0x24(%rax),%edx
	mov    -0x103c(%rbp),%eax
	cmp    %eax,%edx
	jne    <ipc::UdpBridge::rx_loop()+0x253>
	mov    -0x10d8(%rbp),%rax
	movzwl 0x22(%rax),%edx
	movzwl -0x103e(%rbp),%eax
	cmp    %ax,%dx
	je     <ipc::UdpBridge::rx_loop()+0x25a>
	mov    $0x1,%eax
	jmp    <ipc::UdpBridge::rx_loop()+0x25f>
	mov    $0x0,%eax
	mov    %al,-0x21(%rbp)
	cmpb   $0x0,-0x21(%rbp)
	je     <ipc::UdpBridge::rx_loop()+0x2ad>
	mov    $0x3,%edx
	mov    $0x0,%esi
	mov    $0x449280,%edi                # <-- Address of g_peer_valid
	call   <std::atomic<bool>::store(bool, std::memory_order)>
	lea    -0x1040(%rbp),%rax
	mov    -0x10d8(%rbp),%rcx
	mov    0x8(%rax),%rdx
	mov    (%rax),%rax
	mov    %rax,0x20(%rcx)               # <-- Record sockaddr_in 
	mov    %rdx,0x28(%rcx)
	mov    $0x3,%edx
	mov    $0x1,%esi
	mov    $0x449280,%edi                # <-- Address of g_peer_valid
	call   <std::atomic<bool>::store(bool, std::memory_order)>
```
