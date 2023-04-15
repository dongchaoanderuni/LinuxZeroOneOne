@startuml

hide time-axis
concise eax
concise stack
concise edx
concise ds
concise es
concise fs


@0
stack is ss

@1 
stack is esp

@2
stack is eflags

@3
stack is cs 

@4 
stack is eip

@5
eax is "__NR_##name"

@6
stack is ds

@7
stack is es

@8
stack is fs

@9
stack is edx

@10
stack is ecx

@11
stack is ebx

@12
edx is 0x00000010

@13
ds is  0x10

@14
es is 0x10

@15
edx is 0x00000017

@16
fs is 0x17

@17
stack is eax



@enduml
