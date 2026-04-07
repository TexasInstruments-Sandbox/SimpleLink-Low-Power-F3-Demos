        // Flag to prevent circular updates during auto-optimization
        let isAutoUpdating = false;

        // Configuration state
        const config = {
            global: {
                autoOptimize: true,
                lpnCount: 1  // Number of concurrent LPNs (1-10)
            },
            distributor: {
                txSegMax: 10,
                rxSegMax: 10,
                txSegMsgCount: 1,  // Matches lpnCount=1 default
                rxSegMsgCount: 5
            },
            friend: {
                desiredChunksPerRequest: 4,
                queueSize: 46,
                recvWin: 255,
                txSegMax: 10,
                rxSegMax: 10,
                txSegMsgCount: 1,
                rxSegMsgCount: 2
            },
            lpn: {
                rxSegMax: 10,
                txSegMax: 10,
                blobReportTimeout: 10,
                txSegMsgCount: 1,
                rxSegMsgCount: 1,
                lpnRecvDelay: 100,
                lpnPollTimeout: 300
            },
            dfu: {
                firmwareSizeBytes: 262144  // 256 KB in bytes
            }
        };

        // Constants
        const BT_MESH_APP_SEG_SDU_MAX = 12;
        const BLOB_CHUNK_SDU_OVERHEAD = 7;

        // Initialize all event listeners
        function initializeListeners() {
            // Global auto-optimization checkbox
            const globalAutoOptimize = document.getElementById('global-auto-optimize');
            globalAutoOptimize.addEventListener('change', function() {
                config.global.autoOptimize = this.checked;

                // Apply optimizations immediately when enabled
                if (this.checked) {
                    applyAutoOptimizations();
                }

                // Update AUTO badge visibility
                updateAutoBadges();
                updateCalculations();
                updateRecommendations();
            });

            // LPN Count slider
            addListener('lpn-count', 'global', 'lpnCount', 1, 10);

            // Distributor
            addListener('dist-tx-seg', 'distributor', 'txSegMax', 1, 32);
            addListener('dist-rx-seg', 'distributor', 'rxSegMax', 1, 32);
            addListener('dist-tx-count', 'distributor', 'txSegMsgCount', 1, 20);
            addListener('dist-rx-count', 'distributor', 'rxSegMsgCount', 1, 20);

            // Friend
            addListener('friend-desired-chunks', 'friend', 'desiredChunksPerRequest', 2, 12);
            addListener('friend-queue', 'friend', 'queueSize', 8, 512);
            addListener('friend-tx-seg', 'friend', 'txSegMax', 1, 32);
            addListener('friend-rx-seg', 'friend', 'rxSegMax', 1, 32);
            addListener('friend-tx-count', 'friend', 'txSegMsgCount', 1, 10);
            addListener('friend-rx-count', 'friend', 'rxSegMsgCount', 1, 10);

            // LPN
            addListener('lpn-rx-seg', 'lpn', 'rxSegMax', 1, 32);
            addListener('lpn-tx-seg', 'lpn', 'txSegMax', 1, 32);
            addListener('lpn-report-timeout', 'lpn', 'blobReportTimeout', 1, 31);
            addListener('lpn-tx-count', 'lpn', 'txSegMsgCount', 1, 5);
            addListener('lpn-rx-count', 'lpn', 'rxSegMsgCount', 1, 5);

            // DFU - use special handler for number input with validation
            const firmwareSizeInput = document.getElementById('firmware-size-bytes');
            firmwareSizeInput.addEventListener('input', function() {
                let value = parseInt(this.value);
                if (isNaN(value) || value < 1024) value = 1024;
                if (value > 2097152) value = 2097152;
                config.dfu.firmwareSizeBytes = value;
                updateDisplay();
                updateCalculations();
                updateRecommendations();
            });
        }

        function setFirmwareSize(bytes) {
            config.dfu.firmwareSizeBytes = bytes;
            document.getElementById('firmware-size-bytes').value = bytes;
            updateDisplay();
            updateCalculations();
            updateRecommendations();
        }

        function addListener(elementId, nodeType, configKey, min, max) {
            const slider = document.getElementById(elementId);
            const input = document.getElementById(elementId + '-value');

            // Slider input handler
            slider.addEventListener('input', function() {
                const value = parseInt(this.value);
                config[nodeType][configKey] = value;
                input.value = value;

                // Mark as user change, then apply optimizations
                isAutoUpdating = false;
                applyAutoOptimizations();
                updateCalculations();
                updateRecommendations();
            });

            // Number input handler - allow free typing, validate on blur
            input.addEventListener('input', function() {
                let value = parseInt(this.value);

                // Allow empty input or partial input while typing
                if (this.value === '' || isNaN(value)) {
                    return;
                }

                // Update config and slider without clamping or rewriting input
                // This allows users to type intermediate values like "1" when entering "100"
                config[nodeType][configKey] = value;
                slider.value = Math.max(min, Math.min(max, value)); // Clamp slider only

                // Mark as user change, then apply optimizations
                isAutoUpdating = false;
                applyAutoOptimizations();
                updateCalculations();
                updateRecommendations();
            });

            // Blur handler to validate and clamp when done editing
            input.addEventListener('blur', function() {
                let value = parseInt(this.value);

                // Set to min if empty or invalid
                if (this.value === '' || isNaN(value)) {
                    value = min;
                }

                // Clamp to valid range
                if (value < min) value = min;
                if (value > max) value = max;

                // Update everything with the clamped value
                this.value = value;
                config[nodeType][configKey] = value;
                slider.value = value;

                // Mark as user change, then apply optimizations
                isAutoUpdating = false;
                applyAutoOptimizations();
                updateCalculations();
                updateRecommendations();
            });
        }

        function updateDisplay() {
            // Distributor
            document.getElementById('dist-tx-seg-value').value = config.distributor.txSegMax;
            document.getElementById('dist-rx-seg-value').value = config.distributor.rxSegMax;
            document.getElementById('dist-tx-count-value').value = config.distributor.txSegMsgCount;
            document.getElementById('dist-rx-count-value').value = config.distributor.rxSegMsgCount;

            // Friend
            document.getElementById('friend-desired-chunks-value').value = config.friend.desiredChunksPerRequest;
            document.getElementById('friend-queue-value').value = config.friend.queueSize;
            document.getElementById('friend-tx-seg-value').value = config.friend.txSegMax;
            document.getElementById('friend-rx-seg-value').value = config.friend.rxSegMax;
            document.getElementById('friend-tx-count-value').value = config.friend.txSegMsgCount;
            document.getElementById('friend-rx-count-value').value = config.friend.rxSegMsgCount;

            // LPN
            document.getElementById('lpn-rx-seg-value').value = config.lpn.rxSegMax;
            document.getElementById('lpn-tx-seg-value').value = config.lpn.txSegMax;
            document.getElementById('lpn-report-timeout-value').value = config.lpn.blobReportTimeout;
            document.getElementById('lpn-tx-count-value').value = config.lpn.txSegMsgCount;
            document.getElementById('lpn-rx-count-value').value = config.lpn.rxSegMsgCount;

            // DFU
            const sizeKB = (config.dfu.firmwareSizeBytes / 1024).toFixed(1);
            document.getElementById('firmware-size-display').textContent = `${sizeKB} KB`;
        }

        function applyAutoOptimizations() {
            // Prevent circular updates
            if (isAutoUpdating) {
                return;
            }

            isAutoUpdating = true;

            if (config.global.autoOptimize) {
                // BIDIRECTIONAL: Always match exactly (optimization mode)

                // Friend TX/RX should handle max(Distributor TX, LPN TX)
                const requiredFriendTx = Math.max(config.distributor.txSegMax, config.lpn.txSegMax);
                const requiredFriendRx = Math.max(config.distributor.txSegMax, config.lpn.txSegMax);

                if (config.friend.txSegMax !== requiredFriendTx) {
                    config.friend.txSegMax = requiredFriendTx;
                    document.getElementById('friend-tx-seg').value = config.friend.txSegMax;
                }
                if (config.friend.rxSegMax !== requiredFriendRx) {
                    config.friend.rxSegMax = requiredFriendRx;
                    document.getElementById('friend-rx-seg').value = config.friend.rxSegMax;
                }

                // LPN RX should match Distributor TX (receives chunks)
                if (config.lpn.rxSegMax !== config.distributor.txSegMax) {
                    config.lpn.rxSegMax = config.distributor.txSegMax;
                    document.getElementById('lpn-rx-seg').value = config.lpn.rxSegMax;
                }

                // Distributor RX should match LPN TX (receives responses)
                if (config.distributor.rxSegMax !== config.lpn.txSegMax) {
                    config.distributor.rxSegMax = config.lpn.txSegMax;
                    document.getElementById('dist-rx-seg').value = config.distributor.rxSegMax;
                }

            } else {
                // ONE-WAY: Only increase to fix invalid configs (correction mode)

                // Friend TX/RX increase if below max requirement
                const requiredFriendTx = Math.max(config.distributor.txSegMax, config.lpn.txSegMax);
                const requiredFriendRx = Math.max(config.distributor.txSegMax, config.lpn.txSegMax);

                if (config.friend.txSegMax < requiredFriendTx) {
                    config.friend.txSegMax = requiredFriendTx;
                    document.getElementById('friend-tx-seg').value = config.friend.txSegMax;
                }
                if (config.friend.rxSegMax < requiredFriendRx) {
                    config.friend.rxSegMax = requiredFriendRx;
                    document.getElementById('friend-rx-seg').value = config.friend.rxSegMax;
                }

                // LPN RX increases if below Distributor TX
                if (config.lpn.rxSegMax < config.distributor.txSegMax) {
                    config.lpn.rxSegMax = config.distributor.txSegMax;
                    document.getElementById('lpn-rx-seg').value = config.lpn.rxSegMax;
                }

                // Distributor RX increases if below LPN TX
                if (config.distributor.rxSegMax < config.lpn.txSegMax) {
                    config.distributor.rxSegMax = config.lpn.txSegMax;
                    document.getElementById('dist-rx-seg').value = config.distributor.rxSegMax;
                }
            }

            // Friend MSG_COUNT and Distributor TX scaling for multiple LPNs
            const lpnCount = config.global.lpnCount;

            if (config.global.autoOptimize) {
                // BIDIRECTIONAL: Scale to match LPN count
                const requiredFriendTxMsgCount = lpnCount;
                const requiredFriendRxMsgCount = Math.min(10, lpnCount + 1);  // LPNs + 1 Distributor, capped at 10
                const requiredDistTxMsgCount = lpnCount;  // Always match for parallel updates

                if (config.friend.txSegMsgCount !== requiredFriendTxMsgCount) {
                    config.friend.txSegMsgCount = requiredFriendTxMsgCount;
                    document.getElementById('friend-tx-count').value = config.friend.txSegMsgCount;
                }
                if (config.friend.rxSegMsgCount !== requiredFriendRxMsgCount) {
                    config.friend.rxSegMsgCount = requiredFriendRxMsgCount;
                    document.getElementById('friend-rx-count').value = config.friend.rxSegMsgCount;
                }
                if (config.distributor.txSegMsgCount !== requiredDistTxMsgCount) {
                    config.distributor.txSegMsgCount = requiredDistTxMsgCount;
                    document.getElementById('dist-tx-count').value = config.distributor.txSegMsgCount;
                }
            } else {
                // ONE-WAY: Only increase if below required
                const minFriendTxMsgCount = lpnCount;
                const minFriendRxMsgCount = Math.min(10, lpnCount + 1);  // Capped at 10
                const minDistTxMsgCount = lpnCount;

                if (config.friend.txSegMsgCount < minFriendTxMsgCount) {
                    config.friend.txSegMsgCount = minFriendTxMsgCount;
                    document.getElementById('friend-tx-count').value = config.friend.txSegMsgCount;
                }
                if (config.friend.rxSegMsgCount < minFriendRxMsgCount) {
                    config.friend.rxSegMsgCount = minFriendRxMsgCount;
                    document.getElementById('friend-rx-count').value = config.friend.rxSegMsgCount;
                }
                if (config.distributor.txSegMsgCount < minDistTxMsgCount) {
                    config.distributor.txSegMsgCount = minDistTxMsgCount;
                    document.getElementById('dist-tx-count').value = config.distributor.txSegMsgCount;
                }
            }

            // Friend queue size (scaled for multiple LPNs)
            const negotiatedChunkSize = calculateNegotiatedChunkSize();
            const segmentsPerChunk = calculateSegmentsPerChunk(negotiatedChunkSize);

            if (config.global.autoOptimize) {
                // BIDIRECTIONAL: Auto-calculate with safety margin for N LPNs
                const recommendedQueueSize = Math.ceil(
                    config.friend.desiredChunksPerRequest * segmentsPerChunk * 1.15 * lpnCount
                );

                if (Math.abs(config.friend.queueSize - recommendedQueueSize) > 8) {
                    config.friend.queueSize = Math.min(512, Math.max(8, recommendedQueueSize));
                    document.getElementById('friend-queue').value = config.friend.queueSize;
                }
            } else {
                // ONE-WAY: Only increase if below minimum required
                const minQueueSize = Math.ceil(
                    config.friend.desiredChunksPerRequest * segmentsPerChunk * 1.05 * lpnCount
                );

                if (config.friend.queueSize < minQueueSize) {
                    config.friend.queueSize = Math.min(512, Math.max(8, minQueueSize));
                    document.getElementById('friend-queue').value = config.friend.queueSize;
                }
            }

            updateDisplay();
            isAutoUpdating = false;
        }

        function updateAutoBadges() {
            // Update Distributor TX_SEG_MSG_COUNT badge
            const distTxCountBadge = document.getElementById('dist-tx-count-badge');
            const distTxCountTooltip = document.getElementById('dist-tx-count-tooltip');

            if (config.global.autoOptimize) {
                distTxCountBadge.className = 'tooltip info-badge auto-badge';
                distTxCountBadge.textContent = 'AUTO';
                distTxCountTooltip.textContent = `Automatically scaled to ${config.global.lpnCount} for parallel updates to ${config.global.lpnCount} LPNs.`;
            } else {
                distTxCountBadge.className = 'tooltip info-badge auto-badge';
                distTxCountBadge.textContent = 'AUTO';
                distTxCountTooltip.textContent = `Automatically increased to ${config.global.lpnCount} if set lower (one-way correction).`;
            }

            // Update Friend TX_SEG_MSG_COUNT badge
            const friendTxCountBadge = document.getElementById('friend-tx-count-badge');
            const friendTxCountTooltip = document.getElementById('friend-tx-count-tooltip');

            if (config.global.autoOptimize) {
                friendTxCountBadge.className = 'tooltip info-badge auto-badge';
                friendTxCountBadge.textContent = 'AUTO';
                friendTxCountTooltip.textContent = `Automatically scaled to ${config.global.lpnCount} (number of concurrent LPNs).`;
            } else {
                friendTxCountBadge.className = 'tooltip info-badge auto-badge';
                friendTxCountBadge.textContent = 'AUTO';
                friendTxCountTooltip.textContent = `Automatically increased to ${config.global.lpnCount} if set lower (one-way correction).`;
            }

            // Update Friend RX_SEG_MSG_COUNT badge
            const friendRxCountBadge = document.getElementById('friend-rx-count-badge');
            const friendRxCountTooltip = document.getElementById('friend-rx-count-tooltip');

            if (config.global.autoOptimize) {
                friendRxCountBadge.className = 'tooltip info-badge auto-badge';
                friendRxCountBadge.textContent = 'AUTO';
                friendRxCountTooltip.textContent = `Automatically scaled to ${Math.min(10, config.global.lpnCount + 1)} (LPNs + 1 Distributor, capped at 10).`;
            } else {
                friendRxCountBadge.className = 'tooltip info-badge auto-badge';
                friendRxCountBadge.textContent = 'AUTO';
                friendRxCountTooltip.textContent = `Automatically increased to ${Math.min(10, config.global.lpnCount + 1)} if set lower (one-way correction).`;
            }

            // Update Friend TX_SEG_MAX badge
            const friendTxBadge = document.getElementById('friend-tx-seg-badge');
            const friendTxTooltip = document.getElementById('friend-tx-seg-tooltip');

            if (config.global.autoOptimize) {
                friendTxBadge.className = 'tooltip info-badge auto-badge';
                friendTxBadge.textContent = 'AUTO';
                friendTxTooltip.textContent = 'Automatically kept equal to max(Distributor TX, LPN TX). Friend relays messages in both directions.';
            } else {
                friendTxBadge.className = 'tooltip info-badge auto-badge';
                friendTxBadge.textContent = 'AUTO';
                friendTxTooltip.textContent = 'Automatically increased to match max(Distributor TX, LPN TX) if set lower (one-way correction only).';
            }

            // Update Friend RX_SEG_MAX badge
            const friendRxBadge = document.getElementById('friend-rx-seg-badge');
            const friendRxTooltip = document.getElementById('friend-rx-seg-tooltip');

            if (config.global.autoOptimize) {
                friendRxBadge.className = 'tooltip info-badge auto-badge';
                friendRxBadge.textContent = 'AUTO';
                friendRxTooltip.textContent = 'Automatically kept equal to max(Distributor TX, LPN TX). Friend receives from both distributor and LPNs.';
            } else {
                friendRxBadge.className = 'tooltip info-badge auto-badge';
                friendRxBadge.textContent = 'AUTO';
                friendRxTooltip.textContent = 'Automatically increased to match max(Distributor TX, LPN TX) if set lower (one-way correction only).';
            }

            // Update Distributor RX_SEG_MAX badge
            const distRxBadge = document.getElementById('dist-rx-seg-badge');
            const distRxTooltip = document.getElementById('dist-rx-seg-tooltip');

            if (config.global.autoOptimize) {
                distRxBadge.className = 'tooltip info-badge auto-badge';
                distRxBadge.textContent = 'AUTO';
                distRxTooltip.textContent = 'Automatically kept equal to LPN TX_SEG_MAX. Distributor must receive responses from LPN targets.';
            } else {
                distRxBadge.className = 'tooltip info-badge auto-badge';
                distRxBadge.textContent = 'AUTO';
                distRxTooltip.textContent = 'Automatically increased to match LPN TX_SEG_MAX if set lower (one-way correction only).';
            }

            // Update LPN RX_SEG_MAX badge
            const lpnRxBadge = document.getElementById('lpn-rx-seg-badge');
            const lpnRxTooltip = document.getElementById('lpn-rx-seg-tooltip');

            if (config.global.autoOptimize) {
                lpnRxBadge.className = 'tooltip info-badge auto-badge';
                lpnRxBadge.textContent = 'AUTO';
                lpnRxTooltip.textContent = 'Automatically kept equal to Distributor TX_SEG_MAX. LPN receives chunks from distributor.';
            } else {
                lpnRxBadge.className = 'tooltip info-badge auto-badge';
                lpnRxBadge.textContent = 'AUTO';
                lpnRxTooltip.textContent = 'Automatically increased to match Distributor TX_SEG_MAX if set lower (one-way correction only).';
            }

            // Update FRIEND_QUEUE_SIZE badge
            const friendQueueBadge = document.getElementById('friend-queue-badge');
            const friendQueueTooltip = document.getElementById('friend-queue-tooltip');

            if (config.global.autoOptimize) {
                friendQueueBadge.className = 'tooltip info-badge auto-badge';
                friendQueueBadge.textContent = 'AUTO';
                friendQueueTooltip.textContent = `Automatically calculated for ${config.global.lpnCount} concurrent LPN(s). Formula: chunks × segments × 1.15 × ${config.global.lpnCount}.`;
            } else {
                friendQueueBadge.className = 'tooltip info-badge auto-badge';
                friendQueueBadge.textContent = 'AUTO';
                friendQueueTooltip.textContent = `Automatically increased if below minimum for ${config.global.lpnCount} LPN(s) (one-way correction with 5% safety margin).`;
            }
        }

        function calculateNegotiatedChunkSize() {
            // Negotiated chunk size = MIN(all RX_SEG_MAX values) × 12 - BLOB_CHUNK_SDU_OVERHEAD
            // BLOB_CHUNK_SDU_OVERHEAD = opcode(1) + chunk_index(2) + MIC(4) = 7 bytes
            const minRxSegMax = Math.min(
                config.distributor.rxSegMax,
                config.friend.rxSegMax,
                config.lpn.rxSegMax
            );
            return (minRxSegMax * BT_MESH_APP_SEG_SDU_MAX) - BLOB_CHUNK_SDU_OVERHEAD;
        }

        function calculateSegmentsPerChunk(chunkSize) {
            // segments_per_chunk = CEIL((overhead + chunk_size) / 12)
            return Math.ceil((BLOB_CHUNK_SDU_OVERHEAD + chunkSize) / BT_MESH_APP_SEG_SDU_MAX);
        }

        function calculateMaxChunks() {
            const negotiatedChunkSize = calculateNegotiatedChunkSize();
            const segmentsPerChunk = calculateSegmentsPerChunk(negotiatedChunkSize);
            const maxChunksFromQueue = Math.floor(config.friend.queueSize / segmentsPerChunk);
            // Actual max chunks is minimum of:
            // 1. DESIRED_CHUNKS_PER_REQUEST (user preference)
            // 2. maxChunksFromQueue (Friend queue capacity limit)
            return Math.min(config.friend.desiredChunksPerRequest, maxChunksFromQueue);
        }

        function updateCalculations() {
            const negotiatedChunkSize = calculateNegotiatedChunkSize();
            const segmentsPerChunk = calculateSegmentsPerChunk(negotiatedChunkSize);
            const maxChunks = calculateMaxChunks();
            const queueUtil = Math.round((maxChunks * segmentsPerChunk * config.global.lpnCount / config.friend.queueSize) * 100);
            const maxTxSize = (config.distributor.txSegMax * BT_MESH_APP_SEG_SDU_MAX) - 4;
            const maxRxSize = (config.distributor.rxSegMax * BT_MESH_APP_SEG_SDU_MAX) - 4;
            const friendLpnCapacity = Math.min(config.friend.txSegMsgCount, config.friend.rxSegMsgCount);
            const friendMemory = (config.friend.queueSize * 80 + (config.friend.txSegMsgCount + config.friend.rxSegMsgCount) * 200) / 1024;

            // Block and chunk calculations
            const blockSize = 4096; // Standard BLOB block size
            const chunksPerBlock = Math.floor(blockSize / negotiatedChunkSize);
            const totalChunks = Math.ceil(config.dfu.firmwareSizeBytes / negotiatedChunkSize);
            const totalBlocks = Math.ceil(totalChunks / chunksPerBlock);

            document.getElementById('calc-chunk-size').innerHTML =
                `${negotiatedChunkSize}<span class="calc-unit">bytes</span>`;
            document.getElementById('calc-segments-per-chunk').innerHTML =
                `${segmentsPerChunk}<span class="calc-unit">segments</span>`;
            document.getElementById('calc-max-chunks').innerHTML =
                `${maxChunks}<span class="calc-unit">chunks</span>`;
            document.getElementById('calc-queue-util').innerHTML =
                `${queueUtil}<span class="calc-unit">%</span>`;
            document.getElementById('calc-max-tx-size').innerHTML =
                `${maxTxSize}<span class="calc-unit">bytes</span>`;
            document.getElementById('calc-max-rx-size').innerHTML =
                `${maxRxSize}<span class="calc-unit">bytes</span>`;
            // Color-code Friend LPN Capacity based on configured LPN count
            const capacityStyle = friendLpnCapacity >= config.global.lpnCount
                ? 'color: #2e7d32;'  // Green if sufficient
                : 'color: #d32f2f;'; // Red if insufficient
            document.getElementById('calc-friend-lpn-capacity').innerHTML =
                `<span style="${capacityStyle}">${friendLpnCapacity}</span><span class="calc-unit">LPNs</span>`;
            document.getElementById('calc-friend-memory').innerHTML =
                `${friendMemory.toFixed(1)}<span class="calc-unit">KB</span>`;
            document.getElementById('calc-block-size').innerHTML =
                `${blockSize}<span class="calc-unit">bytes</span>`;
            document.getElementById('calc-chunks-per-block').innerHTML =
                `${chunksPerBlock}<span class="calc-unit">chunks</span>`;
            document.getElementById('calc-total-blocks').innerHTML =
                `${totalBlocks}<span class="calc-unit">blocks</span>`;
            document.getElementById('calc-total-chunks').innerHTML =
                `${totalChunks}<span class="calc-unit">chunks</span>`;

            // Calculate and display effective poll rate
            const pollTimeoutMaxMs = calculatePollTimeoutMax();
            const pollTimeoutMaxSec = (pollTimeoutMaxMs / 1000).toFixed(1);
            document.getElementById('calc-poll-rate').innerHTML =
                `${pollTimeoutMaxSec}<span class="calc-unit">seconds (max)</span>`;

            updateDfuDurations();
        }

        function calculatePollTimeoutMax() {
            // Calculate POLL_TIMEOUT_MAX based on Zephyr's actual formula
            // POLL_TIMEOUT_MAX = POLL_TIMEOUT - (REQ_ATTEMPTS × REQ_RETRY_DURATION)
            // Where:
            //   POLL_TIMEOUT = lpnPollTimeout × 100ms
            //   REQ_RETRY_DURATION = LPN_RECV_DELAY + adv_duration + recv_win + 100ms
            //   REQ_ATTEMPTS = MIN(6, POLL_TIMEOUT / REQ_RETRY_DURATION)

            const POLL_TIMEOUT = config.lpn.lpnPollTimeout * 100; // ms
            const LPN_RECV_DELAY = config.lpn.lpnRecvDelay; // ms
            const ADV_DURATION = 30; // ms (approximate, controller-dependent)
            const RECV_WIN = config.friend.recvWin; // ms (from Friend Offer)
            const POLL_RETRY_TIMEOUT = 100; // ms (fixed constant)
            const REQ_ATTEMPTS_MAX = 6;

            const REQ_RETRY_DURATION = LPN_RECV_DELAY + ADV_DURATION + RECV_WIN + POLL_RETRY_TIMEOUT;
            const REQ_ATTEMPTS = Math.min(REQ_ATTEMPTS_MAX, Math.floor(POLL_TIMEOUT / REQ_RETRY_DURATION));
            const POLL_TIMEOUT_MAX = POLL_TIMEOUT - (REQ_ATTEMPTS * REQ_RETRY_DURATION);

            return POLL_TIMEOUT_MAX;
        }

        function calculateDfuDuration(firmwareSizeKB, numTargets) {
            const negotiatedChunkSize = calculateNegotiatedChunkSize();
            const maxChunks = calculateMaxChunks();
            const segmentsPerChunk = calculateSegmentsPerChunk(negotiatedChunkSize);

            // Calculate total chunks needed for firmware
            const firmwareSizeBytes = firmwareSizeKB * 1024;
            const totalChunks = Math.ceil(firmwareSizeBytes / negotiatedChunkSize);

            // Calculate number of block reports needed
            const blockReportsNeeded = Math.ceil(totalChunks / maxChunks);

            // Calculate actual effective poll timeout max using Zephyr's formula
            const pollTimeoutMaxMs = calculatePollTimeoutMax();

            // Time per block report cycle (based on packet capture analysis):
            // 1. LPN sends Block Report requesting maxChunks chunks
            //    - Block Report is unsegmented (9 bytes for 4 chunks: 1 opcode + 4 indices + 4 MIC)
            //    - LPN immediately polls after sending report
            // 2. Immediate poll gets "no data" response: ~100ms
            // 3. LPN waits POLL_TIMEOUT_MAX while Distributor sends chunks to Friend
            //    - During this wait: Distributor → Friend transmission happens (5-6s for 4 chunks)
            //    - Friend queues ALL segments (maxChunks × segments_per_chunk)
            // 4. LPN polls again and begins receiving segments from Friend
            //    - Friend sends ONE segment per poll with md=1 flag
            //    - Each segment delivery (poll + receive + next poll): ~200ms
            //    - Total segment reception: totalSegments × 200ms
            // 5. After receiving all segments, LPN waits BLOB_REPORT_TIMEOUT before next report

            const totalSegments = maxChunks * segmentsPerChunk;

            const immediatePollAfterReportMs = 100;  // Initial poll gets "no data"
            const segmentDeliveryTimeMs = totalSegments * 200;  // 200ms per segment (measured from packet capture)
            const blobReportTimeoutMs = config.lpn.blobReportTimeout * 1000;

            // During active DFU, Zephyr caps poll timeout at 1 second (lpn.c:122)
            // See: if (bt_mesh_tx_in_progress()) return MIN(POLL_TIMEOUT_MAX(lpn), 1 * MSEC_PER_SEC);
            const effectivePollTimeoutMs = Math.min(pollTimeoutMaxMs, 1000);

            // Total time for one block report cycle
            const blockReportCycleMs = immediatePollAfterReportMs + effectivePollTimeoutMs + segmentDeliveryTimeMs + blobReportTimeoutMs;

            // Total time for one LPN (all blocks)
            // Add 30% overhead for retransmissions (SAR default = 2), collisions, ACK/NACK, processing
            // In poor RF conditions, increase to 40-50%. In ideal conditions, may be as low as 20%.
            const OVERHEAD_FACTOR = 1.30;  // 30% for typical RF environment
            const singleLpnTimeMs = blockReportsNeeded * blockReportCycleMs * OVERHEAD_FACTOR;

            // In unicast mode, targets are updated sequentially (one at a time)
            const totalTimeMs = singleLpnTimeMs * numTargets;

            return {
                totalTimeMs: totalTimeMs,
                blockReports: blockReportsNeeded,
                chunksPerReport: maxChunks,
                segmentsPerChunk: segmentsPerChunk,
                totalSegmentsPerReport: totalSegments,
                pollTimeoutMax: (pollTimeoutMaxMs / 1000).toFixed(1)
            };
        }

        function formatDuration(milliseconds) {
            const totalSeconds = Math.round(milliseconds / 1000);
            const hours = Math.floor(totalSeconds / 3600);
            const minutes = Math.floor((totalSeconds % 3600) / 60);
            const seconds = totalSeconds % 60;

            // Format as HH:MM:SS with zero padding
            const hh = String(hours).padStart(2, '0');
            const mm = String(minutes).padStart(2, '0');
            const ss = String(seconds).padStart(2, '0');

            return `${hh}:${mm}:${ss}`;
        }

        function updateDfuDurations() {
            const targetCounts = [config.global.lpnCount];  // Use configured LPN count
            const firmwareSizeKB = config.dfu.firmwareSizeBytes / 1024;

            const grid = document.getElementById('dfu-duration-grid');
            grid.innerHTML = targetCounts.map(numTargets => {
                const duration = calculateDfuDuration(firmwareSizeKB, numTargets);
                const lpnLabel = numTargets === 1 ? '1 LPN' : `${numTargets} LPNs (sequential)`;
                return `
                    <div class="calc-item">
                        <div class="calc-label">${lpnLabel} (${firmwareSizeKB.toFixed(0)} KB each)</div>
                        <div class="calc-value">${formatDuration(duration.totalTimeMs)}<span class="calc-unit">HH:MM:SS</span></div>
                        <div style="font-size: 0.75em; color: #999; margin-top: 5px;">
                            ${duration.blockReports} block reports × ${duration.chunksPerReport} chunks
                        </div>
                    </div>
                `;
            }).join('');
        }

        function updateRecommendations() {
            const recommendations = [];
            const negotiatedChunkSize = calculateNegotiatedChunkSize();
            const segmentsPerChunk = calculateSegmentsPerChunk(negotiatedChunkSize);
            const maxChunks = calculateMaxChunks();
            const lpnCount = config.global.lpnCount;
            const queueUtil = (maxChunks * segmentsPerChunk * lpnCount / config.friend.queueSize);

            // Check chunk size bottleneck
            if (config.lpn.rxSegMax < config.distributor.txSegMax) {
                recommendations.push({
                    type: 'warning',
                    icon: '⚠️',
                    title: 'LPN RX_SEG_MAX is the bottleneck',
                    text: `LPN's RX_SEG_MAX (${config.lpn.rxSegMax}) limits chunk size to ${negotiatedChunkSize} bytes, even though distributor can send ${(config.distributor.txSegMax * 12) - 4} bytes. Consider increasing LPN RX_SEG_MAX to ${config.distributor.txSegMax} for better throughput.`
                });
            }

            // Check Friend queue efficiency (accounting for multiple LPNs)
            if (queueUtil < 0.6) {
                recommendations.push({
                    type: 'warning',
                    icon: '📊',
                    title: 'Friend queue is oversized',
                    text: `Only ${Math.round(queueUtil * 100)}% of Friend queue is utilized for ${lpnCount} LPN(s). Consider reducing FRIEND_QUEUE_SIZE to ${Math.ceil(maxChunks * segmentsPerChunk * 1.2 * lpnCount)} to save memory.`
                });
            } else if (queueUtil > 0.95) {
                recommendations.push({
                    type: 'error',
                    icon: '🚨',
                    title: 'Friend queue is too small',
                    text: `Friend queue is ${Math.round(queueUtil * 100)}% utilized for ${lpnCount} LPN(s), leaving no safety margin. Increase FRIEND_QUEUE_SIZE to at least ${Math.ceil(maxChunks * segmentsPerChunk * 1.25 * lpnCount)} for reliable operation.`
                });
            }

            // Check if chunks per request is optimal (accounting for multiple LPNs)
            if (maxChunks < config.friend.desiredChunksPerRequest) {
                const neededQueueSize = Math.ceil(config.friend.desiredChunksPerRequest * segmentsPerChunk * 1.15 * lpnCount);
                recommendations.push({
                    type: 'warning',
                    icon: '🐢',
                    title: 'Friend queue too small for desired chunks',
                    text: `Friend queue (${config.friend.queueSize}) only supports ${maxChunks} chunks but you want ${config.friend.desiredChunksPerRequest} chunks per request for ${lpnCount} LPN(s). Increase FRIEND_QUEUE_SIZE to at least ${neededQueueSize} or reduce DESIRED_CHUNKS_PER_REQUEST to ${maxChunks}.`
                });
            } else if (maxChunks === config.friend.desiredChunksPerRequest) {
                recommendations.push({
                    type: 'success',
                    icon: '✅',
                    title: 'Perfect configuration!',
                    text: `Friend queue size (${config.friend.queueSize}) is perfectly sized for ${maxChunks} chunks per request for ${lpnCount} LPN(s) with good safety margin.`
                });
            } else if (maxChunks > config.friend.desiredChunksPerRequest + 2) {
                const optimalQueueSize = Math.ceil(config.friend.desiredChunksPerRequest * segmentsPerChunk * 1.15 * lpnCount);
                recommendations.push({
                    type: 'warning',
                    icon: '📊',
                    title: 'Friend queue larger than needed',
                    text: `Friend queue (${config.friend.queueSize}) supports ${maxChunks} chunks but you only want ${config.friend.desiredChunksPerRequest} per request. You can reduce FRIEND_QUEUE_SIZE to ${optimalQueueSize} to save ${Math.round((config.friend.queueSize - optimalQueueSize) * 80 / 1024 * 10) / 10} KB of memory.`
                });
            } else {
                recommendations.push({
                    type: 'success',
                    icon: '✅',
                    title: 'Good configuration',
                    text: `Friend queue supports ${maxChunks} chunks per request (target: ${config.friend.desiredChunksPerRequest}) with appropriate safety margin.`
                });
            }

            // Check Friend TX/RX matching
            if (config.friend.txSegMax < config.distributor.txSegMax ||
                config.friend.rxSegMax < config.distributor.txSegMax) {
                recommendations.push({
                    type: 'error',
                    icon: '❌',
                    title: 'Friend cannot relay large messages',
                    text: `Friend's TX/RX_SEG_MAX should be at least ${config.distributor.txSegMax} to relay messages from distributor to LPN without fragmentation issues.`
                });
            }

            // Check distributor segment message counts (multi-LPN aware)
            if (config.distributor.txSegMsgCount < config.global.lpnCount) {
                recommendations.push({
                    type: 'warning',
                    icon: '⏱️',
                    title: 'Limited concurrent target support',
                    text: `Distributor TX_SEG_MSG_COUNT (${config.distributor.txSegMsgCount}) limits concurrent targets to ${config.distributor.txSegMsgCount}. For updating ${config.global.lpnCount} LPNs in parallel, increase to ${config.global.lpnCount}.`
                });
            }

            // Check Friend TX_SEG_MSG_COUNT (multi-LPN aware)
            const minRequiredFriendTx = config.global.lpnCount;
            if (config.friend.txSegMsgCount < minRequiredFriendTx) {
                recommendations.push({
                    type: 'error',
                    icon: '🚨',
                    title: 'Friend TX insufficient for configured LPNs',
                    text: `Friend TX_SEG_MSG_COUNT (${config.friend.txSegMsgCount}) is insufficient for ${config.global.lpnCount} concurrent LPNs. Increase to at least ${minRequiredFriendTx} to support all targets.`
                });
            } else if (config.friend.txSegMsgCount === 1 && config.global.lpnCount === 1) {
                recommendations.push({
                    type: 'info',
                    icon: 'ℹ️',
                    title: 'Single LPN configuration',
                    text: `Friend TX_SEG_MSG_COUNT of 1 supports 1 LPN at a time. To support multiple LPNs, increase LPN_COUNT and parameters will auto-scale.`
                });
            }

            // Check Friend RX_SEG_MSG_COUNT (multi-LPN aware)
            const minRequiredFriendRx = config.global.lpnCount + 1;  // LPNs + Distributor
            if (config.friend.rxSegMsgCount < minRequiredFriendRx) {
                recommendations.push({
                    type: 'error',
                    icon: '🚨',
                    title: 'Friend RX insufficient for configured LPNs',
                    text: `Friend RX_SEG_MSG_COUNT (${config.friend.rxSegMsgCount}) is insufficient for ${config.global.lpnCount} LPNs + 1 Distributor. Increase to at least ${minRequiredFriendRx}.`
                });
            }

            // Check for imbalance between TX and RX
            if (Math.abs(config.friend.txSegMsgCount - config.friend.rxSegMsgCount) > 2) {
                recommendations.push({
                    type: 'warning',
                    icon: 'ℹ️',
                    title: 'Friend RX/TX MSG_COUNT imbalance',
                    text: `Friend TX_SEG_MSG_COUNT (${config.friend.txSegMsgCount}) and RX_SEG_MSG_COUNT (${config.friend.rxSegMsgCount}) are significantly imbalanced. For ${config.global.lpnCount} LPNs, recommended: TX=${config.global.lpnCount}, RX=${config.global.lpnCount + 1}.`
                });
            }

            // Check if Friend RX is capped due to slider limit
            const idealFriendRx = config.global.lpnCount + 1;
            if (idealFriendRx > 10 && config.friend.rxSegMsgCount === 10) {
                recommendations.push({
                    type: 'warning',
                    icon: '⚠️',
                    title: 'Friend RX capped at maximum',
                    text: `Friend RX_SEG_MSG_COUNT capped at 10 (slider max). Ideal value is ${idealFriendRx} for ${config.global.lpnCount} LPNs. Actual capacity limited to ${config.friend.rxSegMsgCount - 1} concurrent LPNs.`
                });
            }

            // Check if queue size hit 512 limit
            const idealQueueSize = Math.ceil(config.friend.desiredChunksPerRequest * segmentsPerChunk * 1.15 * config.global.lpnCount);
            if (idealQueueSize > 512 && config.friend.queueSize === 512) {
                const actualLpnSupport = Math.floor(512 / (config.friend.desiredChunksPerRequest * segmentsPerChunk * 1.15));
                recommendations.push({
                    type: 'error',
                    icon: '🚨',
                    title: 'Friend queue insufficient for configured LPNs',
                    text: `Friend queue capped at 512 but needs ${idealQueueSize} for ${config.global.lpnCount} LPNs. Current queue only supports ~${actualLpnSupport} LPNs. Reduce LPN count or chunks_per_request.`
                });
            }

            // Check BLOB report timeout vs chunk count
            const reportTimeoutSec = config.lpn.blobReportTimeout;
            if (reportTimeoutSec < 5 && maxChunks > 5) {
                recommendations.push({
                    type: 'warning',
                    icon: '⚡',
                    title: 'Report timeout may be too aggressive',
                    text: `BLOB_REPORT_TIMEOUT of ${reportTimeoutSec}s might not give enough time for ${maxChunks} chunks to arrive. Consider increasing to 10-15 seconds.`
                });
            }

            // All good message
            if (recommendations.filter(r => r.type === 'error' || r.type === 'warning').length === 0) {
                recommendations.push({
                    type: 'success',
                    icon: '🎉',
                    title: 'Configuration looks great!',
                    text: 'All settings are well-balanced for efficient firmware updates. Your configuration should provide reliable and fast DFU transfers.'
                });
            }

            // Memory efficiency check
            const totalMemoryEstimate =
                (config.distributor.txSegMsgCount * 200) +
                (config.distributor.rxSegMsgCount * 200) +
                (config.friend.queueSize * 80) +
                (config.friend.txSegMsgCount * 200) +
                (config.friend.rxSegMsgCount * 200) +
                (config.lpn.txSegMsgCount * 200) +
                (config.lpn.rxSegMsgCount * 200);

            const distMemory = ((config.distributor.txSegMsgCount + config.distributor.rxSegMsgCount) * 200 / 1024).toFixed(1);
            const friendMemory = ((config.friend.queueSize * 80 + (config.friend.txSegMsgCount + config.friend.rxSegMsgCount) * 200) / 1024).toFixed(1);
            const lpnMemory = ((config.lpn.txSegMsgCount + config.lpn.rxSegMsgCount) * 200 / 1024).toFixed(1);

            recommendations.push({
                type: 'info',
                icon: '💾',
                title: 'Estimated memory usage',
                text: `Total estimated RAM: ~${(totalMemoryEstimate / 1024).toFixed(1)} KB (Distributor: ${distMemory} KB, Friend: ${friendMemory} KB [Queue: ${(config.friend.queueSize * 80 / 1024).toFixed(1)} KB + Contexts: ${((config.friend.txSegMsgCount + config.friend.rxSegMsgCount) * 200 / 1024).toFixed(1)} KB], LPN: ${lpnMemory} KB)`
            });

            // Render recommendations
            const container = document.getElementById('recommendations-container');
            container.innerHTML = recommendations.map(rec => `
                <div class="recommendation ${rec.type}">
                    <div class="recommendation-title">
                        <span class="recommendation-icon">${rec.icon}</span>
                        ${rec.title}
                    </div>
                    <div class="recommendation-text">${rec.text}</div>
                </div>
            `).join('');
        }

        function exportConfig() {
            const negotiatedChunkSize = calculateNegotiatedChunkSize();
            const segmentsPerChunk = calculateSegmentsPerChunk(negotiatedChunkSize);
            const maxChunks = calculateMaxChunks();

            const configText = `# BLE Mesh DFU Configuration
# Generated by BLE Mesh DFU Configuration Tool

# ========================================
# DISTRIBUTOR Configuration
# ========================================
CONFIG_BT_MESH_TX_SEG_MAX=${config.distributor.txSegMax}
CONFIG_BT_MESH_RX_SEG_MAX=${config.distributor.rxSegMax}
CONFIG_BT_MESH_TX_SEG_MSG_COUNT=${config.distributor.txSegMsgCount}
CONFIG_BT_MESH_RX_SEG_MSG_COUNT=${config.distributor.rxSegMsgCount}
CONFIG_BT_MESH_BLOB_CLI=y
CONFIG_BT_MESH_DFU_CLI=y
CONFIG_BT_MESH_BLOB_BLOCK_SIZE_MIN=1024

# ========================================
# FRIEND NODE Configuration
# ========================================
CONFIG_BT_MESH_FRIEND=y
CONFIG_BT_MESH_FRIEND_QUEUE_SIZE=${config.friend.queueSize}
CONFIG_BT_MESH_FRIEND_RECV_WIN=${config.friend.recvWin}
CONFIG_BT_MESH_TX_SEG_MAX=${config.friend.txSegMax}
CONFIG_BT_MESH_RX_SEG_MAX=${config.friend.rxSegMax}
CONFIG_BT_MESH_TX_SEG_MSG_COUNT=${config.friend.txSegMsgCount}
CONFIG_BT_MESH_RX_SEG_MSG_COUNT=${config.friend.rxSegMsgCount}
CONFIG_BT_MESH_BLOB_BLOCK_SIZE_MIN=1024

# ========================================
# LPN TARGET Configuration
# ========================================
CONFIG_BT_MESH_LOW_POWER=y
CONFIG_BT_MESH_RX_SEG_MAX=${config.lpn.rxSegMax}
CONFIG_BT_MESH_TX_SEG_MAX=${config.lpn.txSegMax}
CONFIG_BT_MESH_TX_SEG_MSG_COUNT=${config.lpn.txSegMsgCount}
CONFIG_BT_MESH_RX_SEG_MSG_COUNT=${config.lpn.rxSegMsgCount}
CONFIG_BT_MESH_BLOB_SRV=y
CONFIG_BT_MESH_BLOB_SRV_PULL_REQ_COUNT=${config.friend.desiredChunksPerRequest}
CONFIG_BT_MESH_DFU_SRV=y
CONFIG_BT_MESH_BLOB_REPORT_TIMEOUT=${config.lpn.blobReportTimeout}
CONFIG_BT_MESH_LPN_RECV_DELAY=${config.lpn.lpnRecvDelay}
CONFIG_BT_MESH_LPN_POLL_TIMEOUT=${config.lpn.lpnPollTimeout}
`;

            // Copy to clipboard
            navigator.clipboard.writeText(configText).then(() => {
                const button = document.querySelector('.export-button');
                const originalText = button.textContent;
                button.textContent = '✅ Configuration copied to clipboard!';
                button.style.background = 'linear-gradient(135deg, #4caf50, #45a049)';

                setTimeout(() => {
                    button.textContent = originalText;
                    button.style.background = 'linear-gradient(135deg, #667eea, #764ba2)';
                }, 2000);
            }).catch(() => {
                // Fallback: create download
                const blob = new Blob([configText], { type: 'text/plain' });
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = 'mesh_dfu_config.conf';
                a.click();
                URL.revokeObjectURL(url);
            });
        }

        // Initialize on page load
        document.addEventListener('DOMContentLoaded', function() {
            initializeListeners();

            // Apply auto-optimizations first to ensure values match lpnCount
            if (config.global.autoOptimize) {
                applyAutoOptimizations();
            }

            updateDisplay();        // Now syncs the corrected values to HTML
            updateAutoBadges();     // Update badges with correct values
            updateCalculations();   // Calculate metrics
            updateRecommendations(); // Generate recommendations
        });
