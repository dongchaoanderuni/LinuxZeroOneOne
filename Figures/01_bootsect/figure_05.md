@startuml

hide time-axis
concise cs
concise ip
concise ax
concise ds
concise es
concise ss
concise sp

@0
cs is {hidden}
ip is {hidden}
ax is 0x9000
ds is 0x07c0
es is 0x9000

@2
cs is 0x9000
ip is 0x0008

@4
ax is 0x9000

@6 
ds is 0x9000

@8 
es is 0x9000

@10
ss is 0x9000

@12
sp is 0xff00


@enduml
