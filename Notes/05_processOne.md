本节从创建进程1开始

```


 

    if (!fork()) {
        init();
    }

    for(;;) pause();
}
```