#!/bin/bash

echo "Testing Edge Agents..."
echo ""

for i in {1..3}; do
    echo "Testing edge-agent-$i..."
    docker-compose logs --tail=10 edge-agent-$i
    echo ""
done

echo "Checking middleware health..."
curl -s http://localhost:8000/health | jq .
echo ""

echo "Listing active agents..."
curl -s http://localhost:8000/api/v1/agents | jq .
