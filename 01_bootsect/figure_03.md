@startuml

hide time-axis
concise ax
concise ds
concise es
concise cx
concise si
concise di

@0
ax is {hidden}
cx is {hidden}
ds is {hidden}
es is {hidden}
si is {hidden}
di is {hidden}

@2
ax is 0x07c0

@4
ds is 0x07c0

@6
ax is 0x9000

@8
es is 0x9000

@10
cx is 0x0100

@12
si is 0
di is 0

@14
si is 0x0002
di is 0x0002
cx is 0xff

@16
si is 0x0004
di is 0x0004
cx is 0xfe

@18
si is {hidden}
di is {hidden}
cx is {hidden}

@20
si is 0x0200
di is 0x0200
cx is 0x00

@enduml
