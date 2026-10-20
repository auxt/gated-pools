## Gated Pools white paper, Draft v0.3

This is a rewrite around one thesis. A weighted pool rebalances at a band set by its fee plus its arbitrageurs' costs. A gated pool chooses its band, which is why it is cheaper.

**New in v0.3**
- **Cost law for public weighted pools.** L = γ\*·(c + ρ)/(f + c + ρ), with ρ ≈ 0.583·σ·√Δt. It is derived from a stationarity identity and validated within 1.4% (two assets) and 3% (six-token basket, fees ≥ 30 bps).
- **Cost law for gated band rebalancing.** s·K/(δ + δ_in), with the break-even solver spread and the public fee that would match it.
- **Twelve assumptions stress-tested.** Each has a verdict. The thesis fails only if arbitrage is nearly free on blocks of 2 s or less and solvers charge more than 14–30 bps.
- **Oracle risk identified and closed.** Price selection on rebalancing fills would raise the gated cost tenfold. A new on-chain rule closes it: monotone oracle timestamps, one price set per fill, and a 2-second freshness window.
- **Full appendices.** Step-by-step derivations, a worked example for every number, and simulation methods.
- **Reproducible results.** Simulator, scripts and all result data are included.
- **Figures.** Redrawn in black and white with TikZ/pgfplots.

Compared with the earlier, unpublished v0.2 draft, the institutional and legal sections, tokenised stocks/gold/FX, governance and protocol fees, and the token-count study have been removed to keep the paper on its core thesis.

The attached PDF is the paper.
