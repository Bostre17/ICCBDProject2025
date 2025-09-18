// init-mongo.js
db = db.getSiblingDB('visitdb');

db.createCollection('counters');

db.counters.updateOne(
  { _id: "total_visits" },
  { $setOnInsert: { value: 0 } },
  { upsert: true }
);