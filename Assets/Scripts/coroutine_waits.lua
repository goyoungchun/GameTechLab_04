function WaitForSeconds(t)
	if type(t) ~= "number" then
		error("WaitForSeconds input is not number! type(input) = : " .. type(t), 2)
	end
	return { WaitType = "seconds", Seconds = t }
end

function WaitUntil(pred)
	if type(pred) ~= "function" then
		error("WaitUntil input is not function! type(input) = : " .. type(pred), 2)
	end
	return { WaitType = "until", Predicate = pred }
end

function WaitWhile(pred)
	if type(pred) ~= "function" then
		error("WaitWhile input is not function! type(input) = : " .. type(pred), 2)
	end
	return { WaitType = "while", Predicate = pred }
end