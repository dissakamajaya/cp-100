# CP-100 domain route

Public URL: https://houseofexp.com/research/cp-100/

Cloudflare Worker `cp-100-research-route` forwards this path to the existing public ChatGPT Site. Relative asset URLs and PDF downloads stay under the custom path. New Site versions are served automatically.

The wildcard also matches sibling slugs such as `/research/cp-100-notes`; these pass through to the existing `house-of-exp` service. House of EXP cookies, authorization and staff Access headers are not forwarded to the upstream Site. Remote Set-Cookie headers are removed.

## Verify

```sh
node --test worker.test.mjs
```

Deploy through the managed Cloudflare account matching `wrangler.json`. Keep credentials in the environment. To remove this mapping, remove its single Worker route; the existing website handles the path again.
