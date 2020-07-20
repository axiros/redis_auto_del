# Introduction

This is a server side [redis module](https://redis.io/topics/modules-intro).
Its use case is to automatically delete a key from redis when the corresponding
client disconnects.

# Commands

## ax.associate_key

Syntax: `ax.associate_key <key>`

Associates an existing key with the current client session.
When the client disconnects they key gets automatically deleted.

If the key is alreayd associated with a client, this association gets transfered
to the current client calling the command.

If the key gets deleted / expired/ evicted the association is automaticlly cleared.


*Return Values:*

* `0` if the `key` did not exist.
* `1` on success.


# Building

```
git submodule init
git submodule update
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ../
make ax_redis_auto_del
```

After that you got a file called `libax_redis_auto_del.so`.

# Installing

Install the build module via adding the folling line into the redis config file:
`loadmodule <folder>/libax_redis_auto_del.so`
