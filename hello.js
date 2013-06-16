#!nore
console.log("Hello, JavaScript/" + process.title + "!");
console.log("argv:", JSON.stringify(process.argv));

setTimeout(function () {
	console.log("in setTimeout()");
}, 0);
