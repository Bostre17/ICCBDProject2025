#!/bin/bash

echo "⚠️  This will remove all containers, volumes, and networks"
read -p "Are you sure? (y/N) " -n 1 -r
echo

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Cleaning up..."
    docker-compose down -v
    echo "✅ Cleanup complete"
else
    echo "Cancelled"
fi
