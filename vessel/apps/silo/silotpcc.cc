#define CONFIG_H "config/config-perf.h"
#define NDB_MASSTREE 1
#define NO_MYSQL 1
#define USE_JEMALLOC 1
#include <masstree/config.h>

#include <map>
#include <vector>

#include <benchmarks/bench.h>
#include <benchmarks/ndb_wrapper.h>
#include <benchmarks/ndb_wrapper_impl.h>

#include "silotpcc.h"

using namespace std;

// These are hacks to access protected members of classes defined in silo
class tpcc_bench_runner : public bench_runner
{
public:
	tpcc_bench_runner(abstract_db *db);
	vector<bench_loader*> make_loaders(void);
	vector<bench_worker*> make_workers(void);
	map<string, vector<abstract_ordered_index *>> partitions;
};

class my_bench_runner : public tpcc_bench_runner
{
public:
	my_bench_runner(abstract_db *db) : tpcc_bench_runner(db) { }
	vector<bench_loader*> call_make_loaders(void)
	{
		return make_loaders();
	}
	vector<bench_worker*> call_make_workers(void)
	{
		return make_workers();
	}
};

class my_bench_worker : public bench_worker
{
public:
	unsigned int get_worker_id(void)
	{
		return worker_id;
	}

	util::fast_random *get_r(void)
	{
		return &r;
	}

	void call_on_run_setup(void)
	{
		on_run_setup();
	}
};

static abstract_db *db;
static my_bench_runner *runner;
static vector<my_bench_worker *> workers;

extern "C" {

void silotpcc_exec_gc(void)
{
	transaction_proto2_static::PurgeThreadOutstandingGCTasks();
}

int silotpcc_exec_one(int thread_id)
{
	auto worker = workers[thread_id];
	auto workload = worker->get_workload();

	double d = worker->get_r()->next_uniform();
	for (size_t i = 0; i < workload.size(); i++) {
		if ((i + 1) == workload.size() || d < workload[i].frequency) {
			workload[i].fn(worker);
			break;
		}
		d -= workload[i].frequency;
	}
	return 1;
}

int silotpcc_load()
{
	const vector<bench_loader *> loaders = runner->call_make_loaders();
	spin_barrier b(loaders.size());
	std::vector<std::thread> ths;
	for (vector<bench_loader *>::const_iterator it = loaders.begin(); it != loaders.end(); ++it) {
		ths.emplace_back([=, &b] {
			(*it)->set_barrier(b);
			(*it)->start();
			(*it)->join();
		});
	}

	for (auto &w: ths)
		w.join();

	db->do_txn_epoch_sync();
	auto persisted_info = db->get_ntxn_persisted();
	assert(get<0>(persisted_info) == get<1>(persisted_info));
	db->reset_ntxn_persisted();
	persisted_info = db->get_ntxn_persisted();
	ALWAYS_ASSERT(get<0>(persisted_info) == 0 && get<1>(persisted_info) == 0 && get<2>(persisted_info) == 0.0);
	return 0;
}

int silotpcc_init(int number_threads, long numa_memory_)
{
	enable_parallel_loading = 1;
	nthreads = number_threads;
	scale_factor = number_threads;
	pin_cpus = 1;
	verbose = 1;
	long numa_memory = numa_memory_;
	size_t maxpercpu = util::iceil(numa_memory / nthreads, ::allocator::GetHugepageSize());
	::allocator::Initialize(nthreads, maxpercpu);

	vector<string> logfiles;
	vector<vector<unsigned>> assignments;
	int nofsync = 0;
	int do_compress = 0;
	int fake_writes = 0;

	db = new ndb_wrapper<transaction_proto2>(logfiles, assignments, !nofsync, do_compress, fake_writes);
	ALWAYS_ASSERT(!transaction_proto2_static::get_hack_status());

	runner = new my_bench_runner(db);

	silotpcc_load();

	// This is a hack to access protected members of classes defined in silo
	for (auto w: runner->call_make_workers())
		workers.push_back((my_bench_worker *) w);

	return 0;
}

// Hack to access private field coreid::tl_core_id
extern __thread int _ZN6coreid10tl_core_idE;

void silotpcc_init_thread(int thread_id)
{
	auto worker = workers[thread_id];

	// Hack because the first thread of the program becomes a worker
	_ZN6coreid10tl_core_idE = -1;

	// Copy-paste from benchmarks/bench.cc:112
//	coreid::set_core_id(worker->get_worker_id());
//	{
//		scoped_rcu_region r;
//	}
//	worker->call_on_run_setup();
}

}
