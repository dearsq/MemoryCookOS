#ifndef _RCUPDATE_H_
#define _RCUPDATE_H_


#define __rcu 

/*
 * callback structure for use with RCU
 */
struct rcu_head {
	struct rcu_head *next;
	void (*func)(struct rcu_head *head);
};


#define rcu_lockdep_assert(c) do {} while(0)
#define smp_read_barrier_depends() do {} while(0)

#define rcu_dereference_sparse(p,space)  \
	((void)(((typeof(*p) space *)p) == p))

static inline int rcu_read_lock_held(void)
{
	return 1;
}

#define __rcu_dereference_check(p,c,space)      \
	({    \
	 typeof(*p) *____p1 = (typeof(*p) *)ACCESS_ONCE(p);  \
	 rcu_lockdep_assert(c);   \
	 rcu_dereference_sparse(p,space);  \
	 smp_read_barrier_depends();  \
	 ((typeof(*p) *)(____p1));   \
	 })

#define rcu_dereference_check(p,c)  \
	__rcu_dereference_check((p),rcu_read_lock_held() || (c),__rcu)

#define rcu_dereference_raw(p)  rcu_dereference_check(p,1)
#define rcu_read_lock(x)     do {} while(0)
#define rcu_read_unlock(x)   do {} while(0)

#define __rcu_assign_pointer(p,v,space)   \
	({ \
	 (p) = (typeof(*v) space *)(v); \
	 })

/**
 * rcu_assign_pointer() - assign to RCU-protected pointer
 * @p:pointer to assign to
 * @v:value to assign(public)
 *
 * Assigns the specified value to the specified RCU-protected
 * pointer,ensuring that any concurrent RCU readers will see
 * any prior initialization.Returns the value assigned.
 * 
 * Inserts memory barriers on architectures that require them
 * (pretty much all of them other than x86),and also prevents
 * the compiler from recordering the code that initializes the 
 * structure after the pointer assignment.More importantly,this
 * call documents which pointers will be derefreenced by RCU read-side
 * code.
 */
#define rcu_assign_pointer(p,v) \
	__rcu_assign_pointer((p),(v),__rcu)

#endif
