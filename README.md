# user mode concurrency
A collection of concurrency containers and locks in C++ using only light-weight user-mode operations.

Locking usually involves system calls because it requires rescheduling of threads. This introduces latency. For low latency systems where the number of software threads match the number of hardware cores, such latency may be undesirable. In such situations these containers and utilitites may be used with advantage.

The implementations use only C++11 atomics with minimal memory ordering constraints for thread synchronization.
