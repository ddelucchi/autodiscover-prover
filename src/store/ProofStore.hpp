/**
 * @file ProofStore.hpp
 * @brief Content-addressable proof storage with fingerprint-based deduplication
 * 
 * MATHEMATICAL FOUNDATION:
 * ========================
 * 
 * The ProofStore implements a content-addressable storage system where:
 * 
 *   key = fp(canon_bytes)
 * 
 * By the injectivity theorem of -fingerprinting:
 *   fp(B) = fp(B')  B = B'
 * 
 * This guarantees that proof storage and retrieval are sound:
 *   - No false positives: Different proofs have different fingerprints
 *   - No false negatives: Same canonical form always hits cache
 * 
 * DEDUPLICATION GUARANTEE:
 * ========================
 * 
 * For any two proofs P1, P2:
 *   canon(P1) = canon(P2)  ProofStore retrieves the same entry
 * 
 * This enables:
 *   1. Proof reuse across isomorphic subgoals
 *   2. Lemma caching for repeated patterns
 *   3. Incremental proof construction with sharing
 * 
 * @author AutoDiscoverProver Team
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <memory>
#include <shared_mutex>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>

#include "../fingerprint/Fingerprinter.hpp"

namespace autodiscover {
namespace store {

// ===========================================================================
// PROOF METADATA
// ===========================================================================

/**
 * @brief Metadata associated with a stored proof
 */
struct ProofMetadata {
    // Creation timestamp
    std::chrono::steady_clock::time_point createdAt;
    
    // Last access timestamp (for LRU eviction)
    // Stored as atomic int64_t (nanoseconds since epoch) to allow
    // mutation under shared_lock without data races.
    mutable std::atomic<int64_t> lastAccessedNanos{0};
    
    // Access count (for LFU eviction)
    mutable std::atomic<uint64_t> accessCount{0};
    
    // Proof depth (number of inference steps)
    size_t depth;
    
    // Proof size in bytes
    size_t sizeBytes;
    
    // Optional user-defined tag
    std::string tag;
    
    ProofMetadata() 
        : createdAt(std::chrono::steady_clock::now()),
          depth(0),
          sizeBytes(0) {
        auto now = std::chrono::steady_clock::now();
        lastAccessedNanos.store(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                now.time_since_epoch()).count(),
            std::memory_order_relaxed);
    }
    
    ProofMetadata(size_t d, size_t s, const std::string& t = "")
        : createdAt(std::chrono::steady_clock::now()),
          depth(d),
          sizeBytes(s),
          tag(t) {
        auto now = std::chrono::steady_clock::now();
        lastAccessedNanos.store(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                now.time_since_epoch()).count(),
            std::memory_order_relaxed);
    }
    
    // Copy constructor (atomics aren't copyable, so load explicitly)
    ProofMetadata(const ProofMetadata& other)
        : createdAt(other.createdAt),
          depth(other.depth),
          sizeBytes(other.sizeBytes),
          tag(other.tag) {
        lastAccessedNanos.store(other.lastAccessedNanos.load(std::memory_order_relaxed),
                                std::memory_order_relaxed);
        accessCount.store(other.accessCount.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
    }
    
    ProofMetadata& operator=(const ProofMetadata& other) {
        if (this != &other) {
            createdAt = other.createdAt;
            depth = other.depth;
            sizeBytes = other.sizeBytes;
            tag = other.tag;
            lastAccessedNanos.store(other.lastAccessedNanos.load(std::memory_order_relaxed),
                                    std::memory_order_relaxed);
            accessCount.store(other.accessCount.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
        }
        return *this;
    }
    
    /**
     * @brief Record an access (safe to call under shared_lock  all fields are atomic)
     */
    void recordAccess() const {
        auto now = std::chrono::steady_clock::now();
        lastAccessedNanos.store(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                now.time_since_epoch()).count(),
            std::memory_order_relaxed);
        accessCount.fetch_add(1, std::memory_order_relaxed);
    }
    
    /**
     * @brief Get last access time as time_point
     */
    [[nodiscard]] std::chrono::steady_clock::time_point lastAccessedAt() const {
        auto nanos = lastAccessedNanos.load(std::memory_order_relaxed);
        return std::chrono::steady_clock::time_point(
            std::chrono::nanoseconds(nanos));
    }
};

// ===========================================================================
// STORED PROOF ENTRY
// ===========================================================================

/**
 * @brief A single entry in the proof store
 * 
 * Contains the canonical bytecode and the actual proof object.
 * The proof type is parameterized for flexibility.
 */
template <typename Proof>
struct ProofEntry {
    // Canonical bytecode of the proven statement
    std::vector<uint8_t> canonicalBytes;
    
    // The fingerprint (for verification)
    fingerprint::Fingerprint fingerprint;
    
    // The proof object itself
    std::shared_ptr<const Proof> proof;
    
    // Associated metadata
    ProofMetadata metadata;
    
    ProofEntry() = default;
    
    ProofEntry(
        std::vector<uint8_t> canon,
        const fingerprint::Fingerprint& fp,
        std::shared_ptr<const Proof> pf,
        ProofMetadata meta
    ) : canonicalBytes(std::move(canon)),
        fingerprint(fp),
        proof(std::move(pf)),
        metadata(std::move(meta)) {}
};

// ===========================================================================
// PROOF STORE STATISTICS
// ===========================================================================

/**
 * @brief Statistics for proof store operations
 */
struct ProofStoreStats {
    std::atomic<uint64_t> lookups{0};
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> insertions{0};
    std::atomic<uint64_t> evictions{0};
    std::atomic<uint64_t> hashCollisions{0};  // Hash matches but exact differs
    std::atomic<size_t> totalBytes{0};
    
    double hitRate() const {
        uint64_t total = lookups.load(std::memory_order_relaxed);
        if (total == 0) return 0.0;
        return static_cast<double>(hits.load(std::memory_order_relaxed)) / total;
    }
    
    void reset() {
        lookups = 0;
        hits = 0;
        misses = 0;
        insertions = 0;
        evictions = 0;
        hashCollisions = 0;
        totalBytes = 0;
    }
};

// ===========================================================================
// PROOF STORE
// ===========================================================================

/**
 * @brief Content-addressable proof storage with fingerprint-based deduplication
 * 
 * Thread-safe implementation using a shared mutex for read-heavy workloads.
 * 
 * @tparam Proof Type of proof objects stored
 */
template <typename Proof>
class ProofStore {
public:
    using Entry = ProofEntry<Proof>;
    using FindResult = std::optional<std::shared_ptr<const Proof>>;
    
private:
    // The fingerprinter for computing keys
    fingerprint::Fingerprinter fingerprinter_;
    
    // Main storage: Hash256 -> Entry
    // We use the hash as the key for fast lookup, with exact comparison on collision
    struct Bucket {
        std::vector<Entry> entries;
        mutable std::shared_mutex mutex;
    };
    
    static constexpr size_t NUM_BUCKETS = 1024;
    std::array<Bucket, NUM_BUCKETS> buckets_;
    
    // Statistics
    mutable ProofStoreStats stats_;
    
    // Maximum number of entries (0 = unlimited)
    size_t maxEntries_;
    std::atomic<size_t> entryCount_{0};
    
    // Serialise evictions so only one thread evicts at a time
    std::mutex evictionMutex_;
    
public:
    /**
     * @brief Construct a proof store
     * 
     * @param maxExponent Maximum fingerprint exponent (supports 2^maxExponent bytes)
     * @param maxEntries Maximum number of stored proofs (0 = unlimited)
     */
    explicit ProofStore(size_t maxExponent = 24, size_t maxEntries = 0)
        : fingerprinter_(maxExponent), maxEntries_(maxEntries) {}
    
    // -----------------------------------------------------------------------
    // FINGERPRINT ACCESS
    // -----------------------------------------------------------------------
    
    /**
     * @brief Get the fingerprinter for external use
     */
    const fingerprint::Fingerprinter& fingerprinter() const {
        return fingerprinter_;
    }
    
    // -----------------------------------------------------------------------
    // FIND OPERATION
    // -----------------------------------------------------------------------
    
    /**
     * @brief Find a proof by its canonical bytecode fingerprint
     * 
     * @param canonicalBytes Canonical bytecode of the statement
     * @return The stored proof if found, nullopt otherwise
     */
    FindResult find(const std::vector<uint8_t>& canonicalBytes) const {
        stats_.lookups.fetch_add(1, std::memory_order_relaxed);
        
        fingerprint::Fingerprint fp = fingerprinter_.fp_bytes(canonicalBytes);
        return findByFingerprint(fp, canonicalBytes);
    }
    
    /**
     * @brief Find a proof by pre-computed fingerprint
     * 
     * @param fp Pre-computed fingerprint
     * @param canonicalBytes Original bytecode (for exact verification)
     * @return The stored proof if found
     */
    FindResult findByFingerprint(
        const fingerprint::Fingerprint& fp,
        const std::vector<uint8_t>& canonicalBytes
    ) const {
        size_t bucketIdx = fp.hash256.toSizeT() % NUM_BUCKETS;
        const Bucket& bucket = buckets_[bucketIdx];
        
        std::shared_lock lock(bucket.mutex);
        
        for (const Entry& entry : bucket.entries) {
            // Fast path: hash comparison
            if (entry.fingerprint.hash256 == fp.hash256) {
                // Exact verification via fingerprint comparison
                if (entry.fingerprint.exact == fp.exact) {
                    // Final verification: bytecode equality
                    if (entry.canonicalBytes == canonicalBytes) {
                        entry.metadata.recordAccess();
                        stats_.hits.fetch_add(1, std::memory_order_relaxed);
                        return entry.proof;
                    }
                    // Fingerprint exact match but bytecode differs  a genuine collision.
                    // This is extremely unlikely but not impossible. Treat as a miss
                    // and record the collision rather than hard-failing the process.
                    stats_.hashCollisions.fetch_add(1, std::memory_order_relaxed);
                } else {
                    // Hash-only collision (benign)
                    stats_.hashCollisions.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
        
        stats_.misses.fetch_add(1, std::memory_order_relaxed);
        return std::nullopt;
    }
    
    // -----------------------------------------------------------------------
    // INSERT OPERATION
    // -----------------------------------------------------------------------
    
    /**
     * @brief Insert a proof into the store
     * 
     * If a proof with the same fingerprint already exists, returns the existing proof.
     * Otherwise, inserts the new proof and returns it.
     * 
     * @param canonicalBytes Canonical bytecode of the proven statement
     * @param proof The proof to store
     * @param metadata Associated metadata
     * @return The stored proof (may be existing or newly inserted)
     */
    std::shared_ptr<const Proof> insert(
        std::vector<uint8_t> canonicalBytes,
        std::shared_ptr<const Proof> proof,
        ProofMetadata metadata = {}
    ) {
        fingerprint::Fingerprint fp = fingerprinter_.fp_bytes(canonicalBytes);
        return insertWithFingerprint(std::move(canonicalBytes), fp, std::move(proof), std::move(metadata));
    }
    
    /**
     * @brief Insert with pre-computed fingerprint
     */
    std::shared_ptr<const Proof> insertWithFingerprint(
        std::vector<uint8_t> canonicalBytes,
        const fingerprint::Fingerprint& fp,
        std::shared_ptr<const Proof> proof,
        ProofMetadata metadata = {}
    ) {
        size_t bucketIdx = fp.hash256.toSizeT() % NUM_BUCKETS;
        Bucket& bucket = buckets_[bucketIdx];
        
        // First check for existing entry (read lock)
        {
            std::shared_lock lock(bucket.mutex);
            for (const Entry& entry : bucket.entries) {
                if (entry.fingerprint.hash256 == fp.hash256 &&
                    entry.fingerprint.exact == fp.exact &&
                    entry.canonicalBytes == canonicalBytes) {
                    entry.metadata.recordAccess();
                    return entry.proof;
                }
            }
        }
        
        // Evict outside any bucket lock to prevent deadlock:
        // evictOne() takes shared_lock on every bucket to find LRU,
        // then unique_lock on the victim bucket to erase.  If we called
        // it while holding our bucket's unique_lock we would self-deadlock.
        if (maxEntries_ > 0 && entryCount_.load(std::memory_order_relaxed) >= maxEntries_) {
            std::lock_guard<std::mutex> evictGuard(evictionMutex_);
            // Re-check under eviction lock (another thread may have evicted)
            if (entryCount_.load(std::memory_order_relaxed) >= maxEntries_) {
                evictOne();
            }
        }
        
        // Now take write lock on this bucket
        {
            std::unique_lock lock(bucket.mutex);
            
            // Double-check (another thread may have inserted)
            for (const Entry& entry : bucket.entries) {
                if (entry.fingerprint.hash256 == fp.hash256 &&
                    entry.fingerprint.exact == fp.exact &&
                    entry.canonicalBytes == canonicalBytes) {
                    entry.metadata.recordAccess();
                    return entry.proof;
                }
            }
            
            // Insert new entry
            metadata.sizeBytes = canonicalBytes.size();
            bucket.entries.emplace_back(
                std::move(canonicalBytes),
                fp,
                proof,
                std::move(metadata)
            );
            
            entryCount_.fetch_add(1, std::memory_order_relaxed);
            stats_.insertions.fetch_add(1, std::memory_order_relaxed);
            stats_.totalBytes.fetch_add(bucket.entries.back().canonicalBytes.size(), 
                                        std::memory_order_relaxed);
            
            return proof;
        }
    }
    
    // -----------------------------------------------------------------------
    // FIND OR INSERT
    // -----------------------------------------------------------------------
    
    /**
     * @brief Find existing proof or compute and insert a new one
     * 
     * This is the primary API for proof caching:
     *   - If the statement has a cached proof, return it
     *   - Otherwise, call the prover function and cache the result
     * 
     * @param canonicalBytes Canonical bytecode of the statement
     * @param prover Function to compute the proof if not cached
     * @return The proof (cached or newly computed)
     */
    std::shared_ptr<const Proof> findOrInsert(
        std::vector<uint8_t> canonicalBytes,
        std::function<std::shared_ptr<const Proof>()> prover
    ) {
        fingerprint::Fingerprint fp = fingerprinter_.fp_bytes(canonicalBytes);
        
        // Try to find existing
        if (auto existing = findByFingerprint(fp, canonicalBytes)) {
            return *existing;
        }
        
        // Compute new proof
        auto proof = prover();
        if (!proof) return nullptr;
        
        // Insert and return
        return insertWithFingerprint(std::move(canonicalBytes), fp, std::move(proof));
    }
    
    // -----------------------------------------------------------------------
    // EVICTION
    // -----------------------------------------------------------------------
    
    /**
     * @brief Evict one entry (LRU policy)
     * 
     * Finds the least recently accessed entry across all buckets and removes it.
     */
    void evictOne() {
        // Atomically find AND remove the LRU entry in one pass.
        //
        // Previous implementation had a TOCTOU race: it scanned all buckets
        // under shared_lock to find the LRU entry (recording targetBucket and
        // targetIdx), then released the shared lock, then took a unique_lock.
        // Between the two locks, another thread could evict or modify that
        // bucket, making targetIdx stale.
        //
        // Fix: two-pass approach.  Pass 1 finds the candidate bucket under
        // shared_locks.  Pass 2 takes a unique_lock on ONLY that bucket
        // and re-scans it to find the true LRU entry within the bucket.
        // This eliminates the stale-index problem.
        
        // Pass 1: find which bucket contains the oldest entry
        Bucket* targetBucket = nullptr;
        std::chrono::steady_clock::time_point oldestAccess = 
            std::chrono::steady_clock::time_point::max();
        
        for (Bucket& bucket : buckets_) {
            std::shared_lock lock(bucket.mutex);
            for (size_t i = 0; i < bucket.entries.size(); ++i) {
                if (bucket.entries[i].metadata.lastAccessedAt() < oldestAccess) {
                    oldestAccess = bucket.entries[i].metadata.lastAccessedAt();
                    targetBucket = &bucket;
                }
            }
        }
        
        // Pass 2: re-scan the target bucket under unique_lock to find
        // the actual LRU entry (may have changed since Pass 1)
        if (targetBucket) {
            std::unique_lock lock(targetBucket->mutex);
            if (targetBucket->entries.empty()) return;
            
            size_t lruIdx = 0;
            auto lruTime = targetBucket->entries[0].metadata.lastAccessedAt();
            for (size_t i = 1; i < targetBucket->entries.size(); ++i) {
                auto t = targetBucket->entries[i].metadata.lastAccessedAt();
                if (t < lruTime) {
                    lruTime = t;
                    lruIdx = i;
                }
            }
            
            size_t evictedBytes = targetBucket->entries[lruIdx].canonicalBytes.size();
            targetBucket->entries.erase(targetBucket->entries.begin() + 
                                        static_cast<ptrdiff_t>(lruIdx));
            entryCount_.fetch_sub(1, std::memory_order_relaxed);
            stats_.evictions.fetch_add(1, std::memory_order_relaxed);
            stats_.totalBytes.fetch_sub(evictedBytes, std::memory_order_relaxed);
        }
    }
    
    /**
     * @brief Clear all stored proofs
     */
    void clear() {
        for (Bucket& bucket : buckets_) {
            std::unique_lock lock(bucket.mutex);
            bucket.entries.clear();
        }
        entryCount_ = 0;
        stats_.totalBytes = 0;
    }
    
    // -----------------------------------------------------------------------
    // STATISTICS
    // -----------------------------------------------------------------------
    
    /**
     * @brief Get storage statistics
     */
    const ProofStoreStats& stats() const { return stats_; }
    
    /**
     * @brief Get number of stored proofs
     */
    size_t size() const { return entryCount_.load(std::memory_order_relaxed); }
    
    /**
     * @brief Check if store is empty
     */
    bool empty() const { return size() == 0; }
    
    /**
     * @brief Get total bytes stored
     */
    size_t totalBytes() const { 
        return stats_.totalBytes.load(std::memory_order_relaxed); 
    }
};

// ===========================================================================
// EQUATION PROOF STORE
// ===========================================================================

/**
 * @brief Specialized proof store for equation proofs
 * 
 * Uses EquationEncoder for canonical bytecode generation.
 */
template <typename Proof>
class EquationProofStore : public ProofStore<Proof> {
public:
    using ProofStore<Proof>::ProofStore;
    
    /**
     * @brief Find proof by equation terms
     * 
     * Equations are canonically ordered (lhs  rhs) before lookup.
     */
    auto findByEquation(const std::vector<uint8_t>& lhsBytes, 
                        const std::vector<uint8_t>& rhsBytes) const {
        auto eqBytes = encoding::EquationEncoder::encode(lhsBytes, rhsBytes);
        return this->find(eqBytes);
    }
    
    /**
     * @brief Insert proof for equation
     */
    auto insertEquation(
        const std::vector<uint8_t>& lhsBytes,
        const std::vector<uint8_t>& rhsBytes,
        std::shared_ptr<const Proof> proof,
        ProofMetadata metadata = {}
    ) {
        auto eqBytes = encoding::EquationEncoder::encode(lhsBytes, rhsBytes);
        return this->insert(std::move(eqBytes), std::move(proof), std::move(metadata));
    }
};

// ===========================================================================
// PROOF STORE SNAPSHOT
// ===========================================================================

/**
 * @brief Create a read-only snapshot of the proof store
 * 
 * Useful for serialization or debugging.
 */
template <typename Proof>
struct ProofStoreSnapshot {
    std::vector<ProofEntry<Proof>> entries;
    ProofStoreStats stats;
    
    template <typename Store>
    static ProofStoreSnapshot create(const Store& store) {
        ProofStoreSnapshot snapshot;
        snapshot.stats = store.stats();
        
        // This would require iteration support - omitted for brevity
        // In production, add iterator interface to ProofStore
        
        return snapshot;
    }
};

// ===========================================================================
// HIERARCHICAL PROOF STORE
// ===========================================================================

/**
 * @brief Multi-level proof store with local and shared caches
 * 
 * Provides a two-level caching hierarchy:
 *   L1: Local, fast, smaller capacity
 *   L2: Shared, persistent, larger capacity
 */
template <typename Proof>
class HierarchicalProofStore {
private:
    ProofStore<Proof> local_;   // L1: Local cache
    ProofStore<Proof>* shared_; // L2: Shared cache (optional)
    
public:
    /**
     * @brief Construct with local-only storage
     */
    explicit HierarchicalProofStore(size_t localCapacity = 10000)
        : local_(24, localCapacity), shared_(nullptr) {}
    
    /**
     * @brief Construct with shared cache
     */
    HierarchicalProofStore(size_t localCapacity, ProofStore<Proof>* shared)
        : local_(24, localCapacity), shared_(shared) {}
    
    /**
     * @brief Set the shared cache
     */
    void setSharedCache(ProofStore<Proof>* shared) { shared_ = shared; }
    
    /**
     * @brief Find proof in hierarchy (L1 then L2)
     */
    auto find(const std::vector<uint8_t>& canonicalBytes) const {
        // Try L1 first
        if (auto found = local_.find(canonicalBytes)) {
            return found;
        }
        
        // Try L2 if available
        if (shared_) {
            if (auto found = shared_->find(canonicalBytes)) {
                // Optionally promote to L1 here
                return found;
            }
        }
        
        return typename ProofStore<Proof>::FindResult{};
    }
    
    /**
     * @brief Insert into hierarchy (L1 and optionally L2)
     */
    auto insert(
        std::vector<uint8_t> canonicalBytes,
        std::shared_ptr<const Proof> proof,
        ProofMetadata metadata = {}
    ) {
        // Insert into L1
        auto result = local_.insert(canonicalBytes, proof, metadata);
        
        // Also insert into L2 if available
        if (shared_) {
            shared_->insert(std::move(canonicalBytes), std::move(proof), std::move(metadata));
        }
        
        return result;
    }
    
    /**
     * @brief Get local cache statistics
     */
    const ProofStoreStats& localStats() const { return local_.stats(); }
    
    /**
     * @brief Get shared cache statistics (if available)
     */
    const ProofStoreStats* sharedStats() const {
        return shared_ ? &shared_->stats() : nullptr;
    }
};

} // namespace store
} // namespace autodiscover
